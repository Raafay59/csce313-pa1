/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Raafay Hemani
	UIN: 934000608
	Date: 9-28-2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>

using namespace std;

/*
PART 1:
Run the server process as a child of the client process using fork() and exec() such that you do not need two terminals.
The outcome is that you open a single terminal, run the client which first runs the server and then connects to it.
To make sure that the server does not keep running after the client dies, send a QUIT_MSG to the server for each open channel and call the wait(...) function to wait for its end.
Thoughts:
fork from client and start server using exec

PART 2:
For collecting the first 1000 data points for a given patient, request them for both ecg1 and ecg2, collect the responses, and put them in a file named x1.csv. Compare the file against the corresponding data points in the original file and check that they match. Use the following command line.
$ ./client -p <patient number>
Thoughts:
we would only do this if -p is provided but not -t or -e. 
*/


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
		}
	}
	// start server as child process (part 1)
	pid_t pid = fork();
    if (pid == -1) {
		cerr << "Fork failed!" << endl;
		return 1;
	} else if (pid == 0) {
		// exec server
		// server main declaration: int main (int argc, char *argv[])
		char *args[] = {(char*) "./server", NULL}; 
		execvp(args[0], args);
		cerr << "Exec failed!" << endl;
		return 1;
	}

    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
	
	// first thousand lines request
	if (t == -1 && e == -1) {
		cout << "thousand line request" << endl;
		string write_to = "x1.csv";
		ofstream ofs(write_to);
		if (ofs.fail()) {
			EXITONERROR("Cannot open file: " + write_to);
		}
		for (int i = 0; i < 1000; i++) {
			double time = i * 0.004;
			datamsg x1(p, time, 1);
			datamsg x2(p, time, 2);
			char buf1[sizeof(datamsg)];
			char buf2[sizeof(datamsg)];
			memcpy(buf1, &x1, sizeof(datamsg));
			memcpy(buf2, &x2, sizeof(datamsg));
			chan.cwrite(buf1, sizeof(datamsg));
			double reply1;
			chan.cread(&reply1, sizeof(double));
			chan.cwrite(buf2, sizeof(datamsg));
			double reply2;
			chan.cread(&reply2, sizeof(double));
			ofs << time << "," << reply1 << "," << reply2 << endl;
		}
		ofs.close();
	}
	else {
		// example data point request
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e);
		
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}

	
    // sending a non-sense message, you need to change this
	filemsg fm(0, 0);
	string fname = "teslkansdlkjflasjdf.dat";
	
	int len = sizeof(filemsg) + (fname.size() + 1);
	char* buf2 = new char[len];
	memcpy(buf2, &fm, sizeof(filemsg));
	strcpy(buf2 + sizeof(filemsg), fname.c_str());
	chan.cwrite(buf2, len);  // I want the file length;

	delete[] buf2;
	
	// closing the channel    
	// send a QUIT_MSG to the server and wait for the server to exit (part 1)
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));
	int status;
	wait(&status);
	cout << "Client exiting" << endl;
	return 0;
}
