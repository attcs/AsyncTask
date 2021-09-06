# C++ AsyncTask
AsyncTask is a C++ implementation of asynchronous task handling, including cancellation and progress feedback.<br>
The class interface is based on the similarly named Android java class.<br>
[Original AsyncTask reference](https://developer.android.com/reference/android/os/AsyncTask)

## Requirements
* Language standard: C++14 or above
* STL Headers: \<exception\>, \<future\>, \<atomic\>

## Usage
* Asynchronous worker task should be defined by overriding `doInBackground()`
* Check the interruption of the task with the `isCacelled()` in the `doInBackground()`
* Current `Progress` can be stored by `publishProgress()` in `doInBackground()`
* Feedback system elements should be handled by the `onPreExecute()`/`onProgressUpdate()`/`onPostExecute()`/`onCancelled()`
* `execute()` starts the async `doInBackground()`
* On the main thread, using the public `cancel()` function could signal to the `doInBackground()` to interrupt itself.
* Refresh the feedback repeatedly by the `onCallbackLoop()`
* `get()` returns the `Result` of `doInBackground()`

## Notes
* Header only implementation (asynctask.h and the above mentioned standard headers are required to be included).
* Non-copyable object. Instance can be used only once. Inplace reusage can be solved by smart pointers (e.g.: `std::unique_ptr<AsyncTaskChild>` and `std::unique_ptr::reset()`).
* `onCallbackLoop()` and `get()` could rethrow the `doInBackground()` thrown exception. `onCancelled()` would not be executed.
* If the AsyncTask is destructed while background task is running, `~AsyncTask()` will cancel the `doInBackground()` and wait its finish, `onCancelled()` will not be invoked and exception will not be thrown.
* `doInBackground()` could have any number of parameters due to the AsyncTask variadic template definition.
* Unittests are attached. (GTEST)

## Basic example
```C++
    #include "asynctask.h"
    ...
    using Result = string;
    using Progress = int;
    using Input1 = int;
    using Input2 = int;
    
    class EmptyTaskWithProgressFeedback : public AsyncTask<Progress, Result, Input1, Input2>
    {
    protected:

        Result doInBackground(Input1 const& p1, Input2 const& p2) override
        {
            auto const n = p1 + p2;
            for (int i = 0; i <= n; ++i)
            {
                this_thread::sleep_for(chrono::milliseconds(100));
                publishProgress(i);
                
                if (isCancelled()) 
                    return Result("Empty, unfinished object");
            }
            return Result("Finished result object");
        }
  
        void onPreExecute() override { cout << "Time-consuming calculation:\n" << "Progress: 0%"; }
        void onProgressUpdate(Progress const& progress) override { cout << "\rProgress: " << progress << "%"; }
        void onPostExecute(Result const& result) override { cout << "\rProgress is finished."; }
        void onCancelled() override { cout << "\rProgress is cancelled."; }
    };

    int main()
    {
        EmptyTaskWithProgressFeedback pat;
        Input1 p1 = 50;
        Input2 p2 = 50;
        pat.execute(p1, p2); // will invoke doInBackground
        while (!pat.onCallbackLoop())
            this_thread::sleep_for(chrono::milliseconds(120));
        
        Result result = pat.get();
        cout << "\nThe result: " << result;
    }
```
