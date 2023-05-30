# C++ AsyncTask
AsyncTask is a C++ implementation of asynchronous task handling, including cancellation and progress feedback.<br>
The class interface is based on the similarly named Android java class, but avoids and/or elminates its major issues. E.g.: It is not tied to the any UI thread; `doInBackground()`'s exceptions are not swallowed; etc.<br>
[Original AsyncTask reference](https://developer.android.com/reference/android/os/AsyncTask)

## Requirements
* Language standard: C++17 or above (`AsyncTask` is in C++14, `AsyncTaskPQ` is in C++17)
* STL Headers: \<exception\>, \<future\>, \<atomic\>

## Usage
* Asynchronous worker task should be defined by overriding `doInBackground()`
* Check the interruption of the task with the `isCancelled()` in the `doInBackground()`
* Current `Progress` can be stored by `publishProgress()` in `doInBackground()`
* Feedback system elements should be handled by the `onPreExecute()`/`onProgressUpdate()`/`onPostExecute()`/`onCancelled()`
* `execute()` starts the async `doInBackground()`
* On the main thread, using the public `cancel()` function could signal to the `doInBackground()` to interrupt itself.
* Refresh the feedback repeatedly by the `onCallbackLoop()`, it will return `true` if `doInBackground()` is finished. 
* `get()` returns the `Result` of `doInBackground()` and it waits for the result if it has to.
* Inherit from the progress queue solution `AsyncTaskPQ`
  * if multiple progress should be handled in one batch,
  * if the overall progress is not known by the background process, and it can just stepping only,
  * if progress publishing needs thread safe solution,
  * if every published progress items must be handled,
  * and override `isLastShouldBeAltered()` if you want to reduce the number of progress steps by merging the last stored with latest published.

## Notes
* Header only implementation (asynctask.h and the above mentioned standard headers are required to be included).
* Non-copyable object. Instance can be used only once. Inplace reusage can be solved by smart pointers (e.g.: `std::unique_ptr<AsyncTaskChild>` and `std::unique_ptr::reset()`).
* `onCallbackLoop()` and `get()` could rethrow the `doInBackground()`'s exception. In this case, `onCancelled()` would not be executed.
* If the AsyncTask is destructed while background task is running, `~AsyncTask()` will cancel the `doInBackground()` and wait its finish, `onCancelled()` will not be invoked and exception will not be thrown.
* `doInBackground()` could have any number of parameters due to the AsyncTask variadic template definition.
* Unittests are attached. (GTEST)
* Tested compilers: MSVC 2019, Clang 12.0.0, GCC 11.3

## Basic example
```C++
#define _GLIBCXX_USE_NANOSLEEP 

#include "../../asynctask.h"
#include <thread>

#include <iostream>
#include <string>

using namespace std;

using Progress = int;
using InputParam1 = int;
using InputParam2 = int;
using Result = string;
using Exception = string;

class EmptyTaskWithProgressFeedback : public AsyncTask<Progress, Result, InputParam1, InputParam2>
{
protected:

  Result doInBackground(InputParam1 const& p1, InputParam2 const& p2) override
  {
    auto const n = p1 + p2;
    for (int i = 0; i <= n; ++i)
    {
      this_thread::sleep_for(chrono::milliseconds(100)); // simulate Background thread job's work.

      publishProgress(i);

      // throw Exception("Exception message sample"); // -> Comment out to test exception handling

      if (isCancelled()) 
        return "Empty, unfinished object";
    }

    return "Finished result object";
  }
  
  void onPreExecute() override { cout << "Time-consuming calculation:\n" << "Progress: 0%" << flush; }
  void onProgressUpdate(Progress const& progress) override { cout << "\rProgress: " << progress << "%" << flush; }
  void onPostExecute(Result const& result) override { cout << "\rProgress is finished." << flush; }
  void onCancelled() override { cout << "\rProgress is canceled." << flush; }
};

int main()
{
  try
  {
    EmptyTaskWithProgressFeedback asynctask;

    InputParam1 p1 = 50;
    InputParam1 p2 = 50;
    asynctask.execute(p1, p2); // call doInBackground() asynchronously

    for (int nRender = 0; !asynctask.onCallbackLoop(); ++nRender) // if doInBackground() is finished it will stop the loop
    {
      this_thread::sleep_for(chrono::milliseconds(120)); // simulate main thread job's work, e.g.: rendering

      if (nRender > 100) // -> Reduce this number to check Cancellation
        asynctask.cancel();
    }

    cout << "\nThe result: " << asynctask.get() << endl;
  }
  catch (Exception const& e)
  {
    cout << "\nException was thrown: " << e << endl;
  }
}
```
