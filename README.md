# C++ AsyncTask.h
AsyncTask is a C++ implementation of asynchronous task handling, including cancellation and progress feedback.<br>
The class interface is based on the similarly named Android java class.

# Requirements
* Language standard: C++14 or above
* STD Headers: \<exception\>, \<future\>, \<atomic\>

# Usage
* Asynchronous worker task should be defined by overriding doInBackground()
* Check the interruption of the task with the isCacelled() in the doInBackground()
* Current progress can be stored by publishProgress() in doInBackground()
* Feedback system elements should be handled by the onPreExecute()/onProgressUpdate()/onPostExecute()
* Parametrized Ctor() or execute() starts the doInBackground()
* Refresh the feedback repeatedly by the onCallbackLoop()
* get() returns the Result of doInBackground()

# Notes
* Header only implementation.
* Non-copyable object. Instance can be used only once. Inplace reusage can be solved by smart pointers (e.g.: std::unique_ptr<AsyncTaskChild> and std::unique_ptr::reset()).
* onCallbackLoop(), get(), ~AsyncTask() could rethrow the doInBackground() thrown exception. It is based on std::future object.
* If the AsyncTask is destructed while background task is running, ~AsyncTask() will execute the cancellation's logical branch. This could cause temporary freeze until doInBackground() finish, or other troubles, if the onCancelled()'s resources are invalid.
* User interface, logsystem and other servicies can be attached by dependency injection.

# Basic example
    ...
    class EmptyTaskWithProgressFeedback : public AsyncTask<Progress, Result, int, int>
    {
    protected:

        Result doInBackground(int const& p1, int const& p2) override
        {
            auto const n = p1 + p2;
            for (int i = 0; i <= n; ++i)
            {
                this_thread::sleep_for(chrono::milliseconds(100));
                publishProgress(i);
                
                if (isCancelled()) 
                    return "Empty, unfinished object";
            }
            return "Finished result object";
        }
  
        void onPreExecute() override { cout << "Time-consuming calculation:\n" << "Progress: 0%"; }
        void onProgressUpdate(int const& progress) override { cout << "\rProgress: " << progress << "%"; }
        void onPostExecute(Result const& result) override { cout << "\rProgress is finished."; }
    };

    int main()
    {
        EmptyTaskWithProgressFeedback pat;
        pat.execute(50, 50);
        while (!pat.onCallbackLoop())
        {
            chrono::milliseconds timespan(120);
            this_thread::sleep_for(timespan);
        }
        cout << "\nThe result: " << pat.get();
    }

