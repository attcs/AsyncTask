#include <exception>
#include <future>
#include <atomic>
#include "../../AsyncTask.h"

#include <iostream>
#include <string>

using namespace std;

using Progress = int;
using Param1 = int;
using Param2 = int;
using Result = string;
class EmptyTaskWithProgressFeedback : public AsyncTask<Progress, Result, Param1, Param2>
{
protected:

  Result doInBackground(Param1 const& p1, Param1 const& p2) override
  {
    auto const n = p1 + p2;
    for (int i = 0; i <= n; ++i)
    {
      this_thread::sleep_for(chrono::milliseconds(100));
      publishProgress(i);

      //throw string("Exception message sample"); // exception test

      if (isCancelled()) 
        return "Empty, unfinished object";
    }

    return "Finished result object";
  }
  
  void onPreExecute() override { cout << "Time-consuming calculation:\n" << "Progress: 0%"; }
  void onProgressUpdate(int const& progress) override { cout << "\rProgress: " << progress << "%"; }
  void onPostExecute(Result const& result) override { cout << "\rProgress is finished."; }
  void onCancelled() { cout << "\rProgress is cancelled."; }
};

int main()
{
  EmptyTaskWithProgressFeedback pat;
  pat.execute(50, 50);
  try
  {
    while (!pat.onCallbackLoop())
    {
      chrono::milliseconds timespan(120);
      this_thread::sleep_for(timespan);
    }

    cout << "\nThe result: " << pat.get();
  }
  catch (string const& e)
  {
    cout << "\nException was thrown: " << e;
  }
}
