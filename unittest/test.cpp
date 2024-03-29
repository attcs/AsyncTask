#include "pch.h"

#include <exception>
#include <future>
#include <atomic>
#include <vector>
#include <string>

#include "../asynctask.h"

namespace
{
  class LogService
  {
  public:
    enum class Event
    {
      onPreExecute, onProgressUpdate, onPostExecute, onCancelled,
      doInBackground, postResult, publishProgress
    };

  protected:
    std::vector<Event> _events{};

  public:
    void Add(Event&& e) { _events.emplace_back(e); }
    bool Has(Event const& e) { return std::find(_events.begin(), _events.end(), e) != _events.end(); }
  };

  static void Wait(std::chrono::milliseconds const& t = std::chrono::milliseconds(10))
  {
    std::this_thread::sleep_for(t);
  }
}


namespace AsyncTaskTest
{
  class AsyncTaskBasicLog : public AsyncTask<int, int, int>
  {
  private:
    LogService* log = nullptr;

  public:
    AsyncTaskBasicLog(LogService* log) : log(log) {}

  protected:
    void onPreExecute() override { log->Add(LogService::Event::onPreExecute); }
    void onProgressUpdate(int const&) override { log->Add(LogService::Event::onProgressUpdate); }
    void onPostExecute(int const&) override { log->Add(LogService::Event::onPostExecute); }
    void onCancelled() override { log->Add(LogService::Event::onCancelled); }
  };

  namespace MainThread
  {
    class AsyncTaskBasicInt : public AsyncTaskBasicLog
    {
    public:
      AsyncTaskBasicInt(LogService* log) : AsyncTaskBasicLog(log) {}

    protected:
      int doInBackground(int const& n) override
      {
        for (int i = 0; i < n; ++i)
        {
          Wait();
          publishProgress(i);
        }

        return isCancelled() ? 0 : 1;
      }
    };

    TEST(MainThread, getStatus_NotStartedBackgroung_StatusIsPENDING)
    {
      AsyncTaskBasicInt atb(nullptr);

      EXPECT_EQ(AsyncTaskBasicInt::Status::PENDING, atb.getStatus());
    }

    TEST(MainThread, getStatus_StartedBackgroung_StatusIsRUNNING)
    {
      LogService log{};

      AsyncTaskBasicInt::Status s;
      {
        AsyncTaskBasicInt atb(&log);
        atb.execute(5);
        s = atb.getStatus();
      }
      EXPECT_EQ(AsyncTaskBasicInt::Status::RUNNING, s);
    }

    TEST(MainThread, getStatus_StartedBackgroungAndWaited_StatusIsFINISHED)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      atb.get();

      EXPECT_EQ(AsyncTaskBasicInt::Status::FINISHED, atb.getStatus());
    }


    TEST(MainThread, execute_TaskIsAlreadyRunning_ThrowException)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      try
      {
        atb.execute(5);
      }
      catch (AsyncTaskIllegalStateException const& e)
      {
        EXPECT_EQ(AsyncTaskIllegalStateException::eEx::TaskIsAlreadyRunning, e.e);
      }
      catch (...)
      {
        ASSERT_TRUE(false);
      }
    }

    TEST(MainThread, execute_TaskIsAlreadyFinished_ThrowException)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      atb.get();
      try
      {
        atb.execute(5);
      }
      catch (AsyncTaskIllegalStateException const& e)
      {
        EXPECT_EQ(AsyncTaskIllegalStateException::eEx::TaskIsAlreadyFinished, e.e);
      }
      catch (...)
      {
        ASSERT_TRUE(false);
      }
    }


    TEST(MainThread, doInBackground_FinishedGet_1)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      EXPECT_EQ(1, atb.get());
    }


    TEST(MainThread, doInBackground_CancelledGet_0)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      atb.cancel();
      EXPECT_EQ(0, atb.get());
    }


    TEST(MainThread, onPreExecute_RunDuringExecute_LogHasEvent)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      EXPECT_TRUE(log.Has(LogService::Event::onPreExecute));
    }

    TEST(MainThread, onProgressUpdate_RunDuringonCallbackLoop_LogHasEvent)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      while(!atb.onCallbackLoop());
      EXPECT_TRUE(log.Has(LogService::Event::onProgressUpdate));
    }

    TEST(MainThread, onPostExecute_RunDuringFinish_LogHasEvent)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      atb.get();
      EXPECT_TRUE(log.Has(LogService::Event::onPostExecute));
    }

    TEST(MainThread, cancel_isCancelled_True)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      atb.cancel();
      EXPECT_TRUE(atb.isCancelled());
    }

    TEST(MainThread, onCancelled_RunDuringFinish_get_LogHasEvent)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      atb.cancel();
      atb.get();
      EXPECT_TRUE(log.Has(LogService::Event::onCancelled));
    }

    TEST(MainThread, onCancelled_RunDuringFinish_onCallbackLoop_LogHasEvent)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(1);
      atb.cancel();
      while (!atb.onCallbackLoop())
        Wait();

      EXPECT_TRUE(log.Has(LogService::Event::onCancelled));
    }

    TEST(MainThread, onCallbackLoop_Pending_False)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);
      EXPECT_FALSE(atb.onCallbackLoop());
    }

    TEST(MainThread, onCallbackLoop_NotFinished_False)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(5);
      EXPECT_FALSE(atb.onCallbackLoop());
    }

    TEST(MainThread, onCallbackLoop_Finished_True)
    {
      LogService log{};
      AsyncTaskBasicInt atb(&log);

      atb.execute(1);
      atb.get();
      EXPECT_TRUE(atb.onCallbackLoop());
    }
  }

  namespace WorkerThread
  {
    template<typename Progress = int>
    class AsyncTaskWorkerLogProgressTemplate : public AsyncTask<Progress, int, int>
    {
    private:
      LogService* log = nullptr;
      std::chrono::milliseconds const& t;

    public:
      AsyncTaskWorkerLogProgressTemplate(LogService* log, std::chrono::milliseconds const& t = std::chrono::milliseconds(10)) : log(log), t(t) {}

    protected:
      int doInBackground(int const& n) override
      {
        log->Add(LogService::Event::doInBackground);

        for (int i = 0; i < n; ++i)
        {
          Wait(t);
          this->publishProgress(Progress(i));
          if (this->isCancelled())
            return 0;
        }

        return 1;
      }

      void storeProgress(Progress const& p) override
      {
        log->Add(LogService::Event::publishProgress);
        AsyncTask<Progress, int, int>::storeProgress(p);
      }

      int postResult(int&& r) override
      {
        log->Add(LogService::Event::postResult);
        return std::move(r);
      }
    };

    using AsyncTaskWorkerLog = AsyncTaskWorkerLogProgressTemplate<int>;

    TEST(WorkerThread, get_doInBackground_postResult_LogHas)
    {
      LogService log{};

      try
      {
        AsyncTaskWorkerLog at(&log);
        at.execute(0);
        at.get();
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }

      EXPECT_TRUE(log.Has(LogService::Event::doInBackground));
      EXPECT_TRUE(log.Has(LogService::Event::postResult));
    }

    TEST(WorkerThread, publishProgress_StressTest)
    {
      LogService log{};

      struct Progress
      {
        std::vector<int> dataMember;
        std::wstring dataMemberString;

        Progress() = default;
        Progress(int i) { dataMember.resize(1000000); dataMember[1] = i; };
      };
      try
      {
        AsyncTaskWorkerLogProgressTemplate<Progress> at(&log, std::chrono::milliseconds{0});
        at.execute(1000);
        while (!at.onCallbackLoop());
        {
          Wait({});
        }
        at.get();
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }

      EXPECT_TRUE(log.Has(LogService::Event::doInBackground));
      EXPECT_TRUE(log.Has(LogService::Event::postResult));
    }
    
    constexpr int iSurprise = 11;
    struct ProgressExceptionThrowing
    {
      std::vector<int> dataMember;

      ProgressExceptionThrowing() = default;
      ProgressExceptionThrowing(int i) { dataMember.resize(100000); dataMember[1] = i; };

      ProgressExceptionThrowing(ProgressExceptionThrowing const&) { throw iSurprise; };
      ProgressExceptionThrowing& operator= (ProgressExceptionThrowing const&) { throw iSurprise; return *this; };

      ProgressExceptionThrowing(ProgressExceptionThrowing&&) { throw iSurprise; };
      ProgressExceptionThrowing& operator&& (ProgressExceptionThrowing&&) { throw iSurprise; return *this; };

    };

    TEST(WorkerThread, publishProgress_ExceptionAtCopy)
    {
      LogService log{};
      try
      {
        AsyncTaskWorkerLogProgressTemplate<ProgressExceptionThrowing> at(&log, std::chrono::milliseconds{ 0 });
        at.execute(1000);
        at.get();
      }
      catch (decltype(iSurprise) const& ex)
      {
        EXPECT_EQ(ex, iSurprise);
      }
      catch (AsyncTaskIllegalStateException const&)
      {
        EXPECT_TRUE(false);
      }

      EXPECT_TRUE(log.Has(LogService::Event::doInBackground));
      EXPECT_TRUE(log.Has(LogService::Event::publishProgress));
    }


    TEST(WorkerThread, publishProgress_ExceptionAtCopyDtor)
    {
      LogService log{};
      try
      {
        AsyncTaskWorkerLogProgressTemplate<ProgressExceptionThrowing> at(&log, std::chrono::milliseconds{ 0 });
        at.execute(1000);
      }
      catch (decltype(iSurprise) const& ex)
      {
        EXPECT_EQ(ex, iSurprise); // if it is throw that would be
        EXPECT_TRUE(false); // But dtor should not throw anyway.
      }
      catch (AsyncTaskIllegalStateException const&)
      {
        EXPECT_TRUE(false);
      }
    }


    TEST(WorkerThread, get_doInBackground_postResult_publishProgress_LogHas)
    {
      LogService log{};

      try
      {
        AsyncTaskWorkerLog at(&log);
        at.execute(2);
        at.get();
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }

      EXPECT_TRUE(log.Has(LogService::Event::publishProgress));
      EXPECT_TRUE(log.Has(LogService::Event::doInBackground));
      EXPECT_TRUE(log.Has(LogService::Event::postResult));
    }

    TEST(WorkerThread, get_doInBackground_finishProperly_get1)
    {
      LogService log{};

      try
      {
        AsyncTaskWorkerLog at(&log);
        at.execute(2);
        EXPECT_EQ(1, at.get());
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }
    }

    TEST(WorkerThread, get_doInBackground_isCancelledCouldShutDown_get0)
    {
      LogService log{};

      try
      {
        AsyncTaskWorkerLog at(&log);
        at.execute(2);
        at.cancel();
        EXPECT_EQ(0, at.get());
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }
    }

    TEST(WorkerThread, get_postResultDuringDtor_OverrideShouldNotBeInvoked)
    {
      LogService log{};

      try
      {
        AsyncTaskWorkerLog at(&log);
        at.execute(5);
        Wait();
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }

      EXPECT_TRUE(log.Has(LogService::Event::doInBackground));
      EXPECT_FALSE(log.Has(LogService::Event::postResult)); // AsyncTask::postResult should be invoked, not the override.
    }
  }


  namespace ExceptionHandling
  {
    class AsyncTaskException : public AsyncTaskBasicLog
    {
    public:
      AsyncTaskException(LogService* log) : AsyncTaskBasicLog(log) {}

    protected:
      int doInBackground(int const& n) override
      {
        for (int i = 0; i < n; ++i)
          Wait();

        throw 10;
      }
    };

    TEST(ExceptionHandling, doInBackground_ThEx_DtorNoThrow)
    {
      LogService log{};

      try
      {
        AsyncTaskException at(&log);
        at.execute(1);
      }
      catch (...)
      {
        EXPECT_TRUE(false);
      }
    }

    TEST(ExceptionHandling, doInBackground_ThEx_onCallbackLoopRethrow)
    {
      LogService log{};

      try
      {
        AsyncTaskException at(&log);
        at.execute(1);
        while (!at.onCallbackLoop())
          Wait();
      }
      catch (int e)
      {
        EXPECT_EQ(10, e);
      }
    }


    TEST(ExceptionHandling, doInBackground_ThEx_getRethrow)
    {
      LogService log{};

      try
      {
        AsyncTaskException at(&log);
        at.execute(1);
        at.get();
      }
      catch (int e)
      {
        EXPECT_EQ(10, e);
      }
    }

    TEST(ExceptionHandling, doInBackground_ThEx_getStatus_cancel_isCancelled_NotRethrow)
    {
      LogService log{};
      AsyncTaskException at(&log);
      try
      {
        at.execute(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EXPECT_NO_THROW(at.getStatus());
        EXPECT_NO_THROW(at.cancel());
        EXPECT_NO_THROW(at.isCancelled());
      }
      catch (int e)
      {
        EXPECT_EQ(10, e);
      }
    }

  }
}