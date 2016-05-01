#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <map>

using namespace std;

//0 process admitted
//1 process dispatched
//2 io started
//3 io halted
//4 
//5 process completed
//6 time slice up
//7 io ended

int g_te2 = 0;

struct event{
	int end_t;	//event occurrence time 
	int type;	//event type
	int pid;	//process id
	int priority;
	int te;
	event(int et, int t, int pi, int pr) : 
		end_t(et), type(t), pid(pi), priority(pr), te(++g_te2){};
	event() {}
};

bool operator<(const event& e1, const event& e2) {
	if (e1.end_t == e2.end_t) {
		if (e1.type == e2.type) 
			return e1.te < e2.te;
		return e1.type < e2.type;
	}
	return e1.end_t < e2.end_t;
}

class event_mgnr {
	public:
		set<event> event_table;
		map<int, int> p_priorities;
		
	//function for adding an event in the event table
	void add_event(int end_t, int type, int pid)
	{
		event ev;
		ev.end_t = end_t;
		ev.type = type;
		ev.pid = pid;
		ev.priority = p_priorities[pid];
		ev.te = ++g_te2;
		for(set<event>::iterator it = event_table.begin(); it != event_table.end(); it++) {
			if (it->pid == pid)
				event_table.erase(it);
		}
		event_table.insert(ev);
		if (type == 4) {
			if (event_table.find(event(end_t, 10, -1, 100)) == event_table.end())
				event_table.insert(event(end_t, 10, -1, 100));
		}
	}

	//Is event table empty..?
	bool is_empty()
	{
		return event_table.empty();
	}
	
	//function for returning the top_most entry of the event table
	event next_event()
	{
		set<event>::iterator it = event_table.begin();
		event ev = *it;
		event_table.erase(it);
		return ev;
	}

};

