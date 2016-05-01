#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "event_manager.h"
#include <unistd.h>
#include <limits.h>
using namespace std;

event_mgnr em;

struct scheduling_level {
	int level_no;
	int priority;
	int time_slice;	//either some integer or N
};

struct scheduler {
	int no_of_levels;
	set<scheduling_level> levels_arr;
};

bool operator<(scheduling_level s1, scheduling_level s2) {
	if (s1.priority == s2.priority)
		return s1.level_no < s2.level_no;
	return s1.priority < s2.priority;
}

struct process_phase {
	int itrs;	//number of iterations
	int cpu_b;	//cpu burst time
	int io_b;	//IO burst time
};

struct process {
	int pid;
	int start_priority;
	int admission;
	vector<process_phase> phases;
};

scheduler Scheduler;
vector<process> p_list;

map<int, vector<pair<int, int> > > p_unrolled;
map<int, int> p_ptrs;
map<int, int> p_levels; // pid -> level_no
map<int, int> sch_priorities; // sch_priorities[priority] = level_no
map<int, int> sch_slices; // level_no -> time_slice

void handling_PROCESS_SPEC_file(){
	string line, line2;
	int pid, prior;
	int adm;
	int iter;
	int cpu_t, io_t;
	ifstream infile("PROCESS_SPEC");
	while (std::getline(infile, line))
	{
		if(line=="PROCESS") {
			process proc;
			getline(infile, line2);
			std::istringstream iss(line2);
			if (!(iss >> pid >> prior >> adm)) 
				{ break; } 
		        // error

			proc.pid = pid;
			proc.start_priority = prior;
			em.p_priorities[pid] = prior;
			proc.admission = adm;
			p_levels[pid] = sch_priorities[prior];
		//	cout << "pid " << pid << " has priority " << sch_priorities[prior] << endl;
			getline(infile, line2);
			vector<pair<int, int> > unrolled;

			while(line2 != "END"){
				std::istringstream iss(line2);
				process_phase pp;
				if (!(iss >> iter >> cpu_t >> io_t)) 
					{ break; } 
			        // error

				pp.itrs = iter; 
				pp.cpu_b = cpu_t; 
				pp.io_b = io_t; 
				(proc.phases).push_back(pp);
				for(int i = 0; i < iter; i++) {
					unrolled.push_back(make_pair(cpu_t, 0));
					unrolled.push_back(make_pair(io_t, 1));
				}
				getline(infile, line2);
			}
			p_unrolled[pid] = unrolled;
			p_ptrs[pid] = 0;
			p_list.push_back(proc);
			em.add_event(proc.admission,0,proc.pid);	//event type "0" represents "process admission event"
		}
	}
}

int string_to_integer(string str)
{
	int r=1,s=0,l=str.length(),i;
	for(i=l-1;i>=0;i--)
	{
		s = s + ((str[i] - '0')*r);
		r *= 10;
	}
	return s;
}

void handling_SCHEDULER_SPEC_file(){
	string line, line2;
	int level_count;
	int prior;
	int s_lvl;
	int t_slice;
	string t_slice1;
	ifstream infile("SCHEDULER_SPEC");
	while (std::getline(infile, line))
	{
		if(line=="SCHEDULER"){
			getline(infile, line2);
			std::istringstream iss(line2);
		    if (!(iss >> level_count)) { break; } // error

		    Scheduler.no_of_levels = level_count;
		    for(int i=0; i<level_count; i++){
		    	getline(infile, line2);
		    	std::istringstream iss(line2);
				if (!(iss >> s_lvl >> prior >> t_slice1)) { break; } // error
				scheduling_level scl;
				if(t_slice1 == "N")
					t_slice = 9999;
				else
					t_slice = string_to_integer(t_slice1);
				scl.level_no = s_lvl;
				scl.priority = prior;
				scl.time_slice = t_slice;
				sch_priorities[prior] = s_lvl;
				//cout << "prior = " << prior << " level = " << s_lvl << endl;
				Scheduler.levels_arr.insert(scl);
				sch_slices[s_lvl] = t_slice;
			}
		}
	}
}

int g_te = 0;

struct proc{
	int pid;
	int priority;
	int te;
	proc(int p1, int p2) : pid(p1), priority(p2), te(++g_te) {}
};

bool operator<(proc p1, proc p2) {
	if (p1.priority == p2.priority)
		return p1.te < p2.te;
	return p1.priority > p2.priority;
}

set<proc> ready;

int runn=-1;
int curtime=0;
int ld=-1;
int prior=-1;
int main()
{
	handling_SCHEDULER_SPEC_file();
	handling_PROCESS_SPEC_file();
	//processing events
	int iter = 0;

	while(!em.is_empty()) {
	event next;
	next = em.next_event();
	curtime = next.end_t;
	//cout << "curtime = " << curtime << endl;
	switch(next.type)
	{		
		//routine for handling process admission event
		case 0:
		{
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Process Admitted"<<endl;
			ready.insert(proc(next.pid, em.p_priorities[next.pid]));
			if (em.event_table.find(event(next.end_t, 10, -1, 100)) == em.event_table.end())
				em.event_table.insert(event(next.end_t, 10, -1, 100));
 			break;
		}
		case 9:
		{
			int timeslice = sch_slices[p_levels[next.pid]];
			runn=next.pid;
			prior=next.priority;
			ld=curtime;
			for(set<proc>::iterator it = ready.begin(); it != ready.end(); it++ ) {
				if (it->pid == next.pid)
					ready.erase(it);
			}
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"CPU started"<<endl;
			

			
			if(p_unrolled[next.pid][p_ptrs[next.pid]].first<=timeslice)
				em.add_event(curtime+p_unrolled[next.pid][p_ptrs[next.pid]].first,4,next.pid);
			else 
				em.add_event(curtime+timeslice,6,next.pid);
			break;
		}
		case 4:
		{	
			runn=-1; 
			prior=-1;
			
			em.add_event(curtime,1,next.pid);
			p_ptrs[next.pid]++;
			if (em.event_table.find(event(next.end_t, 10, -1, 100)) == em.event_table.end())
				em.event_table.insert(event(next.end_t, 10, -1, 100));
			
			if (sch_slices[p_levels[next.pid]] == curtime - ld) {
				// for demotion
				bool flag = false;
				int curprior = em.p_priorities[next.pid], new_prior = INT_MIN, new_lvl = -1;
				for(set<scheduling_level>::iterator it = Scheduler.levels_arr.begin(); it != Scheduler.levels_arr.end(); it++) {
					if (it->priority < curprior && it->priority > new_prior) {
						new_prior = it->priority;
						new_lvl = it->level_no;
						flag = true;
					}
				}
				if (flag) {
					em.p_priorities[next.pid] = new_prior;
					p_levels[next.pid] = new_lvl;
					cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Process demoted to level " << new_prior <<endl;
				}
				
			}
			else {
				// for promotion
				bool flag = false;
				int curprior = em.p_priorities[next.pid], new_prior = INT_MAX, new_lvl = -1;
				for(set<scheduling_level>::iterator it = Scheduler.levels_arr.begin(); it != Scheduler.levels_arr.end(); it++) {
					if (it->priority > curprior && it->priority < new_prior) {
						new_prior = it->priority;
						new_lvl = it->level_no;
						flag = true;
					}
				}
				if (flag) {
					em.p_priorities[next.pid] = new_prior;
					p_levels[next.pid] = new_lvl;
					cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Process promoted to level " << new_prior <<endl;
				}
			}
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"CPU burst completed	"<<endl;
			break;
		}
		case 1:
		{
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"IO started"<<endl;
			em.add_event(curtime+p_unrolled[next.pid][p_ptrs[next.pid]].first,2,next.pid);

			break;
		}
		case 2:
		{
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"IO burst completed"<<endl;
			p_ptrs[next.pid]++;
			if(p_ptrs[next.pid]>=p_unrolled[next.pid].size()) em.add_event(curtime,5,next.pid);
			else {
				ready.insert(proc(next.pid, em.p_priorities[next.pid]));
				if (em.event_table.find(event(next.end_t, 10, -1, 100)) == em.event_table.end())
					em.event_table.insert(event(next.end_t, 10, -1, 100));
			}
			break;
		}
		case 5:
		{
			runn=-1;
			prior=-1;
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Process terminated"<<endl;
			break;
		}
		case 6:
		{
			runn=-1;
			prior=-1;
			
			p_unrolled[next.pid][p_ptrs[next.pid]].first=p_unrolled[next.pid][p_ptrs[next.pid]].first+ld-curtime;
			
			if (em.event_table.find(event(next.end_t, 10, -1, 100)) == em.event_table.end())
				em.event_table.insert(event(next.end_t, 10, -1, 100));
			
		
			// for demotion
			bool flag = false;
			int curprior = em.p_priorities[next.pid], new_prior = INT_MIN, new_lvl = -1;
			for(set<scheduling_level>::iterator it = Scheduler.levels_arr.begin(); it != Scheduler.levels_arr.end(); it++) {
				if (it->priority < curprior && it->priority > new_prior) {
					new_prior = it->priority;
					new_lvl = it->level_no;
					flag = true;
				}
			}
			if (flag) {
				em.p_priorities[next.pid] = new_prior;
				p_levels[next.pid] = new_lvl;
				cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Process demoted to level " << new_prior <<endl;
			}
			ready.insert(proc(next.pid, em.p_priorities[next.pid]));
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Time slice ended"<<endl;
			break;
		}
		case 7:
		{
			runn=-1;
			prior=-1;
			cout<<"PID :: "<<next.pid<<"  TIME :: "<<next.end_t<<"  EVENT :: "<<"Process pre-empted"<<endl;
			p_unrolled[next.pid][p_ptrs[next.pid]].first=p_unrolled[next.pid][p_ptrs[next.pid]].first+ld-curtime;
			ready.insert(proc(next.pid, em.p_priorities[next.pid]));
			break;
		}
		case 10:
		{	
		//	cout << "case 10, curtime = " << curtime <<endl;
		//	cout << "ready = " << ready.size() << endl;
			if(ready.size()!=0)
			{
				if(runn==-1)
				{
					em.add_event(curtime,9, ready.begin()->pid);
				}
				else if (prior < ready.begin()->priority){
					em.add_event(curtime, 7, runn);
					em.add_event(curtime, 9, ready.begin()->pid);
				}
			}
			break;
	  	}		
	}
	}	
		
	return 0;
}
