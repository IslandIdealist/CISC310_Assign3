#include "process.h"

// Process class methods
Process::Process(ProcessDetails details, uint32_t current_time)
{
    int i;
    pid = details.pid;
    start_time = details.start_time;
    num_bursts = details.num_bursts;
    entry_time = 0;
    current_burst = 0;
    burst_times = new uint32_t[num_bursts];
    for (i = 0; i < num_bursts; i++)
    {
        burst_times[i] = details.burst_times[i];
    }
    priority = details.priority;
    state = (start_time == 0) ? State::Ready : State::NotStarted;
    if (state == State::Ready)
    {
        launch_time = current_time;
    }
    core = -1;
    turn_time = 0;
    wait_time = 0;
    cpu_time = 0;
    remain_time = 0;
    for (i = 0; i < num_bursts; i+=2)
    {
        remain_time += burst_times[i];
    }
}

uint32_t Process::getBurstTimes() const
{
	return *burst_times;
}

Process::~Process() 
{
    delete[] burst_times;
}

uint16_t Process::getCurrBurstIndex() const
{
    return current_burst;
}

uint16_t Process::getPid() const
{
    return pid;
}

uint32_t Process::getStartTime() const
{
    return start_time;
}

uint8_t Process::getPriority() const
{
    return priority;
}

Process::State Process::getState() const
{
    return state;
}

int8_t Process::getCpuCore() const
{
    return core;
}

double Process::getTurnaroundTime() const
{
    return (double)turn_time / 1000.0;
}

double Process::getWaitTime() const
{
    return (double)wait_time / 1000.0;
}

double Process::getCpuTime() const
{
    return (double)cpu_time / 1000.0;
}

double Process::getRemainingTime() const
{
    return (double)remain_time / 1000.0;
}

int32_t Process::getEntryTime() const
{
	return entry_time;
}

uint32_t Process::getCurrBurst() const
{
	return burst_times[current_burst];
}

uint16_t Process::getNumBursts() const
{
	return num_bursts;
}


void Process::incrementCurrBurst()
{
	current_burst = current_burst + 1;
}


void Process::setState(State new_state, uint32_t current_time)
{
    if (state == State::NotStarted && new_state == State::Ready)
    {
        launch_time = current_time;
    }
	if(new_state == State::IO){
		entry_time = current_time;
	}

    state = new_state;
}

void Process::setCpuCore(int8_t core_num)
{
    core = core_num;
}

void Process::updateCpuTime(uint32_t additionalTime){

	cpu_time = additionalTime;
}

void Process::updateTurnTime(uint32_t additionalTime){

	turn_time = additionalTime;
}

void Process::updateBurstTime(int burst_idx, uint32_t new_time)
{
    burst_times[burst_idx] = new_time;
}

void Process::updateRemainingTime(int32_t new_time)
{
	remain_time = new_time;
}

/*void Process::updateProcess(uint32_t current_time)
{
    // use `current_time` to update turnaround time, wait time, burst times, 
    // cpu time, and remaining time
    wait_time = current_time;
    turn_time = current_time;
}*/


// Comparator methods: used in std::list sort() method

// SJF - comparator for sorting read queue based on shortest remaining CPU time
bool SjfComparator::operator ()(const Process *p1, const Process *p2)
{

	if(p2->getRemainingTime() < p1->getRemainingTime()){
		return false;
	} else {
		return true;
	}
}

// PP - comparator for sorting read queue based on priority
bool PpComparator::operator ()(const Process *p1, const Process *p2)
{
	if(p2->getPriority() < p1->getPriority()){
		return false;
	} else {
		return true;
	}
}
