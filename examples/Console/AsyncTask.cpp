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
      this_thread::sleep_for(chrono::milliseconds(100));
      publishProgress(i);

      // throw Exception("Exception message sample"); // Comment out to test exception handling

      if (isCancelled()) 
        return "Empty, unfinished object";
    }

    return "Finished result object";
  }
  
  void onPreExecute() override { cout << "Time-consuming calculation:\n" << "Progress: 0%" << flush; }
  void onProgressUpdate(int const& progress) override { cout << "\rProgress: " << progress << "%" << flush; }
  void onPostExecute(Result const& result) override { cout << "\rProgress is finished." << flush; }
  void onCancelled() override { cout << "\rProgress is canceled." << flush; }
};

int main()
{
  EmptyTaskWithProgressFeedback asynctask;

  InputParam1 p1 = 50;
  InputParam1 p2 = 50;
  asynctask.execute(p1, p2);
  try
  {
    for (int nRender = 0; !asynctask.onCallbackLoop(); ++nRender)
    {
      this_thread::sleep_for(chrono::milliseconds(120));
      if (nRender > 100) // Reduce this number to check Cancellation
        asynctask.cancel();
    }

    cout << "\nThe result: " << asynctask.get() << endl;
  }
  catch (Exception const& e)
  {
    cout << "\nException was thrown: " << e << endl;
  }
}
