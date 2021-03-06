// Required STD headers
// #include <exception>
// #include <future>
// #include <atomic>


class IllegalStateException : std::exception
{
public:
  enum class eEx : int { TaskIsAlreadyRunning, TaskIsAlreadyFinished };

  eEx e;

  IllegalStateException() = default;
  IllegalStateException(eEx e) : e(e) {}
};


// AsyncTask 
// Asynchronous task progress handler class
//  - Asynchronous worker task should be defined into the doInBackground(), 
//  - Feedback system elements should be handled by the onPreExecute()/onProgressUpdate()/onPostExecute()/onCancelled()
//  - execute() start the doInBackground()
//  - Refresh the feedback by the onCallbackLoop()
// Nocopy object. onCallbackLoop() and get() could rethrow the doInBackground() thrown exceptions.
template<typename Progress, typename Result, typename... Params>
class AsyncTask
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
  Result mResult{};
  std::future<Result> mFuture{}; // Future is a non-copyable object so AsyncTask also.
  std::atomic<Progress> atProgress{};
  std::atomic_bool atCancelled{};
  std::atomic_bool isExceptionRethrowNeededOnMainThread = false;
  std::exception_ptr eptr;

public:
  AsyncTask() = default;
  
protected:
  AsyncTask(AsyncTask const&) = delete;
  AsyncTask(AsyncTask&&) = delete;
  AsyncTask operator=(AsyncTask const&) = delete;
  AsyncTask operator=(AsyncTask&&) = delete;
  virtual ~AsyncTask() noexcept
  {
    auto const status = getStatus();
    if (status != Status::RUNNING)
      return;

    if (!isCancelled())
      cancel();

    if (!mFuture.valid())
      return;

    mFuture.wait();
    // Exception rethrow: No. It could terminate the program if others already threw.
  };

public:

  // Initiate the asynchronous task
  // If the task is already began, IllegalStateException will be thrown
  // @MainThread
  AsyncTask<Progress, Result, Params...>& execute(Params const&... params)
  {
    switch (mStatus)
    {
      case Status::PENDING: break; // Everything is ok.
      case Status::RUNNING: throw IllegalStateException(IllegalStateException::eEx::TaskIsAlreadyRunning);
      case Status::FINISHED: throw IllegalStateException(IllegalStateException::eEx::TaskIsAlreadyFinished);
    }

    mStatus = Status::RUNNING;

    onPreExecute();
    mFuture = std::async(std::launch::async,
      [this](Params const&... params) -> Result
      { 
        if (isCancelled())
          return {}; // Protect against undefined behavoiur, if Dtor is invoked before the - pure virtual function represented - task would be started

        try { return postResult(doInBackground(params...)); }
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

  // Post process result if it is needed, still on the worker thread
  // @WorkerThread
  virtual Result postResult(Result&& result) { return result; }

  // Usually to setup the feedback system
  // @MainThread
  virtual void onPreExecute() {}

  // Usually to declare the finishing in the feedback system
  // @MainThread
  virtual void onPostExecute(Result const& result) {}

  // Show progress in the feedback system
  // @MainThread
  virtual void onProgressUpdate(Progress const& values) {}

  // Store the current state of the progress inside the class
  // Use inside the doInBackground()
  // @WorkerThread
  virtual void publishProgress(Progress const& progress)
  {
    if (isCancelled())
      return;

    atProgress.store(progress);
  }

  // Cleanup function if the task is cancelled
  // @MainThread
  virtual void onCancelled() {}

  // Cleanup function if the task is cancelled
  // @MainThread
  virtual void onCancelled(Result const& result) { onCancelled(); }

public:
  // Returns true if the task is cancelled by the cancel()
  // It is usable to break process inside the doInBackground()
  // @MainThread and @Workerthread
  bool isCancelled() const noexcept { return atCancelled.load(); }

  // Cancel the task
  // @MainThread
  void cancel() noexcept { atCancelled.store(true); }

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
        onProgressUpdate(atProgress.load());
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
