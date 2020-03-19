#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include "configreader.h"
#include "process.h"
#include <time.h>
#include <bits/stdc++.h>

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex mutex;
    std::condition_variable condition;
    ScheduleAlgorithm algorithm;
    uint32_t context_switch;
    uint32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
} SchedulerData;

void coreRunProcesses(uint8_t core_id, SchedulerData *data);
int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex);
void clearOutput(int num_lines);
#include<iterator>


uint32_t currentTime();
std::string processStateToString(Process::State state);

int main(int argc, char **argv)
{
    // ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(1);
    }

    // declare variables used throughout main
    int i;
    SchedulerData *shared_data;
    std::vector<Process*> processes;

    // read configuration file for scheduling simulation
    SchedulerConfig *config = readConfigFile(argv[1]);

    // store configuration parameters in shared data object
    uint8_t num_cores = config->cores;
    shared_data = new SchedulerData();
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;

    // create processes
    uint32_t start = currentTime();
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i], start);
        processes.push_back(p);
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p);
        }
    }

    // free configuration data from memory
    deleteConfig(config);

    // launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }

	unsigned int startTime = currentTime();

	std::vector<Process*>::iterator itr;

    // main thread work goes here:
    int num_lines = 0;
    while (!(shared_data->all_terminated))
    {
        // clear output from previous iteration
        clearOutput(num_lines);

		for(itr = processes.begin(); itr < processes.end(); itr++){

			//Turnaround time tracker
			if((*itr)->getState() != Process::State::NotStarted && (*itr)->getState() != Process::State::Terminated){

				(*itr)->updateTurnTime(currentTime()-startTime - (*itr)->getStartTime());
			}

			// start new processes at their appropriate start time
			if((*itr)->getState() == Process::State::NotStarted && (*itr)->getStartTime() + startTime < currentTime()){ 
			//Is process not started and is it time to start it?

				(*itr)->setState(Process::State::Ready, currentTime()); //Set state to ready
				shared_data->ready_queue.push_back(*itr); //Put back on ready queue
				
				//printf("Process added to ready queue.\n");
			}
		}

        // determine when an I/O burst finishes and put the process back in the ready queue

		for(itr = processes.begin(); itr < processes.end(); itr++){
			if((*itr)->getState() == Process::State::IO && (*itr)->getCurrBurst()+ (*itr)->getEntryTime() < currentTime()){ 
			//Is process in I/O and has elapsed time passed?

				(*itr)->setState(Process::State::Ready, currentTime()); //Set state back to ready
				(*itr)->incrementCurrBurst(); //Move to next index of bursts
				shared_data->ready_queue.push_back(*itr); //Put back on ready queue

				//printf("Process I/O burst done; added back to ready queue.\n");
			}
		}

        // sort the ready queue (if needed - based on scheduling algorithm)

		if(shared_data->algorithm == ScheduleAlgorithm::SJF){
			shared_data->ready_queue.sort(SjfComparator());
		}
		if(shared_data->algorithm == ScheduleAlgorithm::PP){
			shared_data->ready_queue.sort(PpComparator());
		}

        // determine if all processes are in the terminated state

		bool terminated = true;

		for(itr = processes.begin(); itr < processes.end(); itr++){
		//Check each process to see if it's been terminated.
			if((*itr)->getState() != Process::State::Terminated){
				terminated = false;
			}
		}

		if(terminated == true){ //Ends loop if all are terminated.
			shared_data->all_terminated = true;
		}

        // output process status table
        num_lines = printProcessOutput(processes, shared_data->mutex);

        // sleep 1/60th of a second
        usleep(16667);
    }

    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i].join();
    }

    // print final statistics
    //  - CPU utilization

	int totalTime = 0;

	for(itr = processes.begin(); itr < processes.end(); itr ++){
			
		totalTime = totalTime + (*itr)->getCpuTime();
	}

	printf("CPU Utilization: %lf percent,\n", (double)((startTime - currentTime())/totalTime));

    //  - Throughput

	double turnaroundTimes[processes.size()];
	
	int j = 0;
	for(itr = processes.begin(); itr < processes.end(); itr++){
		
		turnaroundTimes[j] = (*itr)->getTurnaroundTime();
		j++;
	}

	std::sort(turnaroundTimes, turnaroundTimes + processes.size());

    //     - Average for first 50% of processes finished

	totalTime = 0;

	for(int i = 0; i < (sizeof(turnaroundTimes)/sizeof(turnaroundTimes[1]))/2; i ++){

		totalTime = totalTime + turnaroundTimes[i];
	}

	//printf("Average throughput for first half: %d process per %lf ms,\n" ((sizeof(turnaroundTimes)/sizeof(turnaroundTimes[1]))/2), totalTime);

    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time

	totalTime = 0;

	for(itr = processes.begin(); itr < processes.end(); itr ++){

		totalTime = totalTime + (*itr)->getTurnaroundTime();
	}
	printf("Average turnaround time: %lf ms,\n", (double)(totalTime/processes.size()));

    //  - Average waiting time

	totalTime = 0;

	for(itr = processes.begin(); itr < processes.end(); itr ++){

		totalTime = totalTime + (*itr)->getWaitTime();
	}

	printf("Average wait time: %lf ms,\n", (double)(totalTime/processes.size()));	

    // Clean up before quitting program
    processes.clear();

    return 0;
}

void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
    //  - Simulate the processes running until one of the following:
    //     - CPU burst time has elapsed
    //     - RR time slice has elapsed
    //     - Process preempted by higher priority process
    //  - Place the process back in the appropriate queue
    //     - I/O queue if CPU burst finished (and process not finished)
    //     - Terminated if CPU burst finished and no more bursts remain
    //     - Ready queue if time slice elapsed or process was preempted
    //  - Wait context switching time
    //  * Repeat until all processes in terminated state

	bool processChecker = false;
	Process *currentProcess;

	{
	    //  - Get process at front of ready queue
		std::lock_guard<std::mutex> lock(shared_data->mutex);
		currentProcess = shared_data->ready_queue.front(); //pop the top item off the queue
		shared_data->ready_queue.pop_front();
	}
	
	printf("%u is the currentProcess burst.\n", currentProcess->getCurrBurst());

	unsigned int start = currentTime(); //start the timing
	while( processChecker == false )
	{
		currentProcess->setState(Process::State::Running, currentTime());
		currentProcess->setCpuCore(core_id);

		unsigned int stop = currentTime();
		unsigned int timePassed = (stop - start);
		currentProcess->updateBurstTime(currentProcess->getCurrBurstIndex(), currentProcess->getCurrBurst()-timePassed);

		printf("CurrBurst is %u,\n", currentProcess->getCurrBurst());

		if( (shared_data->ready_queue.size() != 0) && (shared_data->algorithm == ScheduleAlgorithm::PP) )
		{
			if( shared_data->ready_queue.front()->getPid() < currentProcess->getPid()) //check if the top of the queue has a higher priorty than the current running process
			{
					/*while( completed == false )
					{
						shared_data->mutex.lock();
						while( (shared_data->condtion % shared_data->ready_queue.size()) == 1 ) //Maybe?
						{
							shared_data->mutex.unlock();
							shared_data->mutex.lock();
							//write data
							currentProcess->updateTurn(1?);
							completed = true;
						}
					}*/
					unsigned int stop = currentTime(); //stop the clock
					unsigned int timePassed = (stop - start); //generate the time duration
					//currentProcess->updateProcess(timePassed); //sets the amount of time that has been completed so far
					currentProcess->setState(Process::State::Ready, currentTime());  //sets the current process back to ready
					currentProcess->updateBurstTime(currentProcess->getCurrBurstIndex(), currentTime());
					shared_data->ready_queue.push_back(currentProcess); //inserts the current process back into the queue
					currentProcess->incrementCurrBurst();
					currentProcess = shared_data->ready_queue.front(); //pops the new "higher" priority process off the stack
		
					shared_data->ready_queue.pop_front();
				
			}
		}
		//printf("%u is current time passed.\n", stop);		
		if( currentProcess->getCurrBurst() <= 0) //checks to see if the process has completed in normal process.
		{
			
			currentProcess->setCpuCore(-1);
			currentProcess->setState(Process::State::IO, currentTime()); //sets the current process to IO state

			currentProcess->updateBurstTime(currentProcess->getCurrBurstIndex(), 0); //the cpu burst is over
			currentProcess->incrementCurrBurst(); //move onto next burst for IO cycle


			processChecker == true; //breaks out of the loop
			usleep(shared_data->context_switch * 1000);
		}	
	} 	
}

int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex)
{
    int i;
    int num_lines = 2;
    std::lock_guard<std::mutex> lock(mutex);
    printf("|   PID | Priority |      State | Core | Turn Time | Wait Time | CPU Time | Remain Time |\n");
    printf("+-------+----------+------------+------+-----------+-----------+----------+-------------+\n");
    for (i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double turn_time = processes[i]->getTurnaroundTime();
            double wait_time = processes[i]->getWaitTime();
            double cpu_time = processes[i]->getCpuTime();
            double remain_time = processes[i]->getRemainingTime();
            printf("| %5u | %8u | %10s | %4s | %9.1lf | %9.1lf | %8.1lf | %11.1lf |\n", 
                   pid, priority, process_state.c_str(), cpu_core.c_str(), turn_time, 
                   wait_time, cpu_time, remain_time);
            num_lines++;
        }
    }
    return num_lines;
}

void clearOutput(int num_lines)
{
    int i;
    for (i = 0; i < num_lines; i++)
    {
        fputs("\033[A\033[2K", stdout);
    }
    rewind(stdout);
    fflush(stdout);
}

uint32_t currentTime()
{
    uint32_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}
