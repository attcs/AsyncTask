/*  
MIT License

Copyright (c) 2020 Attila Csikós (attcs)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <exception>
#include <atomic>
#include <mutex>
#include <type_traits>
#include <queue>

#ifdef _MSC_VER
#pragma warning(suppress : 4355)
#include <future>
#else
#include <future>
#endif

class AsyncTaskIllegalStateException
{
public:
  enum class eEx : int { TaskIsAlreadyRunning, TaskIsAlreadyFinished };

  eEx e;

  AsyncTaskIllegalStateException() = default;
  AsyncTaskIllegalStateException(eEx e) : e(e) {}
};



// AsyncTask 
// Asynchronous task progress handler class
//  - Asynchronous worker task should be defined into the doInBackground(), 
//  - Feedback system elements should be handled by the onPreExecute()/onProgressUpdate()/onPostExecute()/onCancelled()
//  - execute() start the doInBackground()
//  - Refresh the feedback by the onCallbackLoop()
// Nocopy object. onCallbackLoop() and get() could rethrow the doInBackground() thrown exceptions.
template<typename Progress, typename Result, typename... Params>
class AsyncTaskBase
{
public:
  enum class Status : int
  {
    PENDING, // Indicates that the task has not been executed yet.
    RUNNING, // Indicates that the task is running.
    FINISHED // Indicates that the task is finished.
  };

private:
  Status mStatus = Status::PENDING;

  // Result handling
  Result mResult{};
  std::future<Result> mFuture{}; // Future is a non-copyable object so AsyncTask also.

  // Cancellation handling
  std::atomic_bool atCancelled{};

  // Exception handling
  std::atomic_bool isExceptionRethrowNeededOnMainThread = { false };
  std::exception_ptr eptr;

public:
  AsyncTaskBase() = default;
  
protected:
  AsyncTaskBase(AsyncTaskBase const&) = delete;
  AsyncTaskBase(AsyncTaskBase&&) = delete;
  AsyncTaskBase& operator=(AsyncTaskBase const&) = delete;
  AsyncTaskBase& operator=(AsyncTaskBase&&) = delete;
  virtual ~AsyncTaskBase() noexcept
  {
    auto const status = getStatus();
    if (status != Status::RUNNING)
      return;

    if (!this->isCancelled())
      this->cancel();

    if (!this->mFuture.valid())
      return;
    
    this->mFuture.wait();
    // Exception rethrow: No. It could terminate the program if others already threw.
  };

public:

  // Initiate the asynchronous task
  // If the task is already began, AsyncTaskIllegalStateException will be thrown
  // @MainThread
  AsyncTaskBase<Progress, Result, Params...>& execute(Params const&... params) noexcept(false)
  {
    switch (mStatus)
    {
      case Status::PENDING: break; // Everything is ok.
      case Status::RUNNING: throw AsyncTaskIllegalStateException(AsyncTaskIllegalStateException::eEx::TaskIsAlreadyRunning);
      case Status::FINISHED: throw AsyncTaskIllegalStateException(AsyncTaskIllegalStateException::eEx::TaskIsAlreadyFinished);
    }

    this->mStatus = Status::RUNNING;

    this->onPreExecute();
    this->mFuture = std::async(std::launch::async,
      [this](Params const&... params) -> Result
      { 
        if (isCancelled())
          return {}; // Protect against undefined behavoiur, if Dtor is invoked before the - pure virtual function represented - task would be started

        try { return this->postResult(this->doInBackground(params...)); }
        catch (...)
        {
          cancel();
          isExceptionRethrowNeededOnMainThread.store(true);
          eptr = std::current_exception();
        }
        return {};
      }, 
      params...);

    return *this;
  }

  // @MainThread
  Status getStatus() const noexcept { return mStatus; }

protected:

  // Background worker task
  // Exception can be thrown, it will be rethrown in get(), onCallbackLoop() or Dtor()
  // @WorkerThread
  virtual Result doInBackground(Params const&... params) = 0;

  // Define the store mechanism of the current state of the progress inside the class
  // @WorkerThread
  virtual void storeProgress(Progress const&) {}

  // Store the current state of the progress inside the class
  // Use inside the doInBackground()
  // @WorkerThread
  void publishProgress(Progress const& progress)
  {
    // doInBackground() could invoke publishProgress() during dtor(), so publishProgress() must be a non-virtual function and prevent to call virtual ones using isCancelled()
    if (isCancelled())
      return;

    this->storeProgress(progress);
  }

public:
  // Post process result if it is needed, still on the worker thread
  // @WorkerThread
  virtual Result postResult(Result&& result) { return std::move(result); }

  // Usually to setup the feedback system
  // @MainThread
  virtual void onPreExecute() {}

  // Usually to declare the finishing in the feedback system
  // @MainThread
  virtual void onPostExecute(Result const&) {}

  // Cleanup function if the task is canceled
  // @MainThread
  virtual void onCancelled() {}

  // Cleanup function if the task is canceled
  // @MainThread
  virtual void onCancelled(Result const&) { onCancelled(); }

protected:
  virtual void handleProgress() = 0;

public:
  // Returns true if the task is canceled by the cancel()
  // It is usable to break process inside the doInBackground()
  // @MainThread and @Workerthread
  bool isCancelled() const noexcept { return atCancelled.load(std::memory_order_relaxed); }

  // Cancel the task
  // @MainThread
  void cancel() noexcept { atCancelled.store(true, std::memory_order_relaxed); }

  // Get the result.
  // It could freeze the mainthread if it invoked before the task is finished. Exception from the doInBackground can be rethrown.
  //@MainThread
  Result get()
  {
    if (getStatus() != Status::FINISHED)
      finish(mFuture.get());

    return mResult;
  }

  // Callback loop to refresh progress in the feedback system
  // Return true if the task is finished. Exception from the doInBackground can be rethrown.
  // @MainThread
  bool onCallbackLoop()
  {
    if (mStatus == Status::FINISHED)
      return true;

    if (mStatus == Status::PENDING || !mFuture.valid())
      return false;

    auto const statusThread = mFuture.wait_for(std::chrono::seconds(0));

    switch (statusThread)
    {
      case std::future_status::deferred:
        return false;

      case std::future_status::timeout:
        handleProgress();
        return false;

      case std::future_status::ready:
        finish(mFuture.get());
        return true;

      default:
        return false;
    }
  }

private:
  // @MainThread
  void finish(Result&& result)
  {
    mResult = result;
    if (isCancelled())
      onCancelled(mResult);
    else
      onPostExecute(mResult);

    mStatus = Status::FINISHED;

    if (isExceptionRethrowNeededOnMainThread.load() && eptr)
      std::rethrow_exception(eptr);
  }
};


// General AsyncTask
template<typename Progress, typename Result, typename... Params>
class AsyncTask : public AsyncTaskBase<Progress, Result, Params...>
{
private:
  // Progress handling
  template<typename Data>
  struct ThreadSafeContainer
  {
  private:
    Data mData{};
    mutable std::mutex mMutex{};

  public:
    ThreadSafeContainer() = default;
    ThreadSafeContainer(ThreadSafeContainer const&) = delete;
    ThreadSafeContainer(ThreadSafeContainer&&) = delete;
    ThreadSafeContainer& operator=(ThreadSafeContainer const&) = delete;
    ThreadSafeContainer& operator=(ThreadSafeContainer&&) = delete;

    void store(Data const& data)
    {
      std::unique_lock<std::mutex> lock(mMutex);
      mData = data;
    }

    Data load() const
    {
      std::unique_lock<std::mutex> lock(mMutex);
      return mData;
    }
  };

  static bool constexpr isProgressAtomicCompatible = std::is_trivially_copyable_v<Progress>
    && std::is_copy_constructible_v<Progress>
    && std::is_move_constructible_v<Progress>
    && std::is_copy_assignable_v<Progress>
    && std::is_move_assignable_v<Progress>;

  using ProgressContainer = typename std::conditional<isProgressAtomicCompatible
    , std::atomic<Progress>
    , ThreadSafeContainer<Progress>
  >::type;

  ProgressContainer mProgress;

protected:

  // Store the current state of the progress inside the class
  // @WorkerThread
  void storeProgress(Progress const& progress) override
  {
    this->mProgress.store(progress);
  }

  // Show progress in the feedback system
  // @MainThread
  virtual void onProgressUpdate(Progress const&) {}

protected:
  virtual void handleProgress() override final
  {
    this->onProgressUpdate(mProgress.load());
  }

public:
  using AsyncTaskBase<Progress, Result, Params...>::AsyncTaskBase;
};


// AsyncTaskPQ: AsyncTask using Progress Queue to handle progress queue in the proper order and handle every published progress item
template<typename Progress, typename Result, typename... Params>
class AsyncTaskPQ : public AsyncTaskBase<Progress, Result, Params...>
{
private:
  // Progress handling
  template<typename Data>
  struct ThreadSafeQueue
  {
  private:
    std::queue<Data> mData{};
    mutable std::mutex mMutex{};

  public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(ThreadSafeQueue const&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue const&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

    template<typename BinaryOp>
    void store(Data const& data, BinaryOp fnLastShouldBeOverride)
    {
      std::unique_lock<std::mutex> lock(mMutex);
      if (mData.empty() || !fnLastShouldBeOverride(mData.back(), data))
        mData.push(data);
      else
        mData.back() = data;
    }


    std::queue<Data> move()
    {
      std::unique_lock<std::mutex> lock(mMutex);
      auto const mDataMoved = std::move(mData);
      return mDataMoved;
    }
  };

  ThreadSafeQueue<Progress> mProgressQueue;

protected:

  // To define condition when not all progress item wanted to be stored.
  // @WorkerThread
  virtual bool isLastProgressShouldBeOverride(Progress const& progressOld, Progress const& progressNew) const { return false; }
  
  // Store the current state of the progress inside the class
  // @WorkerThread
  void storeProgress(Progress const& progress) override
  {
    // or use overrideLast() in special cases.
    this->mProgressQueue.store(progress, [this](Progress const& progressOld, Progress const& progressNew) 
    { 
      return this->isLastProgressShouldBeOverride(progressOld, progressNew);
    });
  }

  // Show progress in the feedback system
  // @MainThread
  virtual void onProgressUpdate(Progress const&) {}

protected:
  virtual void handleProgress() override final
  {
    for (auto progressQueue = mProgressQueue.move(); !progressQueue.empty(); progressQueue.pop())
      this->onProgressUpdate(progressQueue.front());
  }

public:
  using AsyncTaskBase<Progress, Result, Params...>::AsyncTaskBase;
};
