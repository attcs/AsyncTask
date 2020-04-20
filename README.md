# C++ AsyncTask.h
AsyncTask is a C++ implementation of asynchronus task handling, including cancellation and progress feedback.
The class interface is based on the similarly named Android java class.

# Requirements
Language standard: C++14 or above
STD Headers: <exception>, <future>, <atomic>

# Usage
* Asynchronous worker task should be defined by overriding doInBackground()
* Check the interruption of the task with the isCacelled() in the doInBackground()
* Current progress can be stored using publishProgress() by doInBackground()
* Feedback system elements should be handled by the onPreExecute()/onProgressUpdate()/onPostExecute()
* Parametrized Ctor() or execute() starts the doInBackground()
* Refresh the feedback repeatedly by the onCallbackLoop()
* get() returns the Result of doInBackground()

# Notes
Header only implementation.
Non-copyable object. Instance can be used only once. If the reusages needed, handle by a smart ptr (e.g.: std::unique_ptr<AsyncTaskChild> and std::unique_ptr::reset()).
onCallbackLoop(), get(), ~AsyncTask() could rethrow the doInBackground() thrown exception. It is based on std::future object.
If the AsyncTask is destructed while background task is running, ~AsyncTask() will execute the cancellation logical branch. This could cause temporary freeze until doInBackground() finish, or other troubles if the onPostExecute()'s resources are invalid.
User interface, logsystem and other servicies can be attached by dependency injection.

