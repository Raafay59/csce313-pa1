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

PART 3:
RequestT5 a file from the server side using the following command format again using getopt(...) function:
$ ./client -f <file name>
The file does not need to be one of the .csv files currently existing in the BIMDC directory. You can put any file in the BIMDC/ directory and request it from the directory. The steps for requesting a file are as follows.
First, send a file message to get its length.
Next, send a series of file messages to get the content of the file.
Put the received file under the received/ directory with the same name as the original file.
Compare the received file against the original file using the Linux command diff and demonstrate that they are identical.
Measure the time for the transfer for different file sizes (you may use the Linux command
$ truncate -s ⟨s⟩ test.bin
to truncate an existing file test.bin to s bytes, or to create a file with s NULL bytes. Tabulate the time taken to transfer files of various sizes and put the results in the report as a chart.
MakeT6 sure to treat the file as binary because we will use this same program to transfer any type of file (e.g., music files, ppt, and pdf files, which are not necessarily made of ASCII text). Putting the data in an STL string will not work because C++ strings are NULL terminated. To demonstrate that your file transfer is capable of handling binary files, make a large empty file under the BIMDC/ directory using the truncate command (see man pages on how to use truncate), transfer that file, and then compare to make sure that they are identical using the diff command.
Experiment T7 with transferring a large file (100MB), and document the required time. What is the main bottleneck here? Submit your answer in the repository in a file answer.txt.
THOUGHTS:
first make a directory received if it does not exist
*/

int main(int argc, char *argv[])
{
	try
	{
		// mkdir received
		if (mkdir("received", 0777) == -1)
		{
			if (errno != EEXIST)
			{
				EXITONERROR("Cannot create directory received/");
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << '\n';
	}

	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = -1;
	bool requested_channel = false;
	vector<FIFORequestChannel*> channels;

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1)
	{
		switch (opt)
		{
		case 'p':
			p = atoi(optarg);
			break;
		case 't':
			t = atof(optarg);
			break;
		case 'e':
			e = atoi(optarg);
			break;
		case 'f':
			filename = optarg;
			break;
		case 'm':
			m = atoi(optarg);
			break;
		case 'c':
			requested_channel = true;
			break;
		}
	}
	if (m < 1 || m > MAX_MESSAGE)
	{
		m = MAX_MESSAGE;
	}
	// start server as child process (part 1)
	pid_t pid = fork();
	if (pid == -1)
	{
		cerr << "Fork failed!" << endl;
		return 1;
	}
	else if (pid == 0)
	{
		// exec server
		// server main declaration: int main (int argc, char *argv[])
		char *args[] = {(char *)"./server", (char *)"-m", (char *)to_string(m).c_str(), NULL};
		execvp(args[0], args);
		cerr << "Exec failed!" << endl;
		return 1;
	}

	FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);
	if (requested_channel) {
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
		char buf[30];
		cont_chan.cread(buf, 30);
		string new_channel_name = string(buf);
		FIFORequestChannel* new_chan = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(new_chan);
	}
	FIFORequestChannel chan = *(channels.back());

	if (filename != "")
	{
		// file request from BIMDC server dir
		/*
		First, send a file message to get its length.
		Next, send a series of file messages to get the content of the file.
		Put the received file under the received/ directory with the same name as the original file.
		dummy example:
			// sending a non-sense message, you need to change this
			filemsg fm(0, 0);
			string fname = "teslkansdlkjflasjdf.dat";

			int len = sizeof(filemsg) + (fname.size() + 1);
			char* buf2 = new char[len];
			memcpy(buf2, &fm, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), fname.c_str());
			chan.cwrite(buf2, len);  // I want the file length;

			delete[] buf2;
		*/
		// first get size of target file
		filemsg fm(0, 0); // offset, length
		string fname = filename;
		int len = sizeof(filemsg) + (fname.size() + 1);
		char *buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);
		delete[] buf2;
		// request file in chunks of size m
		__int64_t file_length;
		chan.cread(&file_length, sizeof(__int64_t));
		int num_of_requests = ceil((double)file_length / m);
		ofstream ofs("received/" + filename, ios::binary);
		if (ofs.fail())
		{
			EXITONERROR("Cannot open file: received/" + filename);
		}
		for (int i = 0; i < num_of_requests; i++)
		{
			__int64_t offset = i * m;
			int length = ((i == num_of_requests - 1) ? (file_length - offset) : m);
			filemsg fm2(offset, length);
			string fname2 = filename;
			int len2 = sizeof(filemsg) + (fname2.size() + 1);
			char *buf3 = new char[len2];
			memcpy(buf3, &fm2, sizeof(filemsg));
			strcpy(buf3 + sizeof(filemsg), fname2.c_str());
			chan.cwrite(buf3, len2);
			delete[] buf3;
			char *file_buf = new char[length];
			chan.cread(file_buf, length);
			ofs.write(file_buf, length);
			delete[] file_buf;
		}
		ofs.close();
	}
	// first thousand lines request
	else if (t == -1 && e == -1 && p != -1)
	{
		cout << "thousand line request" << endl;
		string write_to = "received/x1.csv";
		ofstream ofs(write_to);
		if (ofs.fail())
		{
			EXITONERROR("Cannot open file: " + write_to);
		}
		for (int i = 0; i < 1000; i++)
		{
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
	else if (p != -1)
	{
		// example data point request
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e);

		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); // answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}

	MESSAGE_TYPE q = QUIT_MSG;
	if (requested_channel) {
		chan.cwrite(&q, sizeof(MESSAGE_TYPE));
		delete channels.back();
		channels.pop_back();
	}
	// closing the channel
	cont_chan.cwrite(&q, sizeof(MESSAGE_TYPE));
	return 0;
}
