// mmu.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <unistd.h>

using namespace std;

typedef struct {
	unsigned int VALID : 1;
	unsigned int WRITE_PROTECTED : 1;
	unsigned int MODIFIED : 1;
	unsigned int REFERENCED : 1;
	unsigned int PAGEDOUT : 1;
	unsigned int framenum : 7;
	unsigned int vms : 1; // indicates whether it is a hole(1)
	unsigned int filemap : 2; // indicates whether it is filemapped
}pte_t;

typedef struct {
	unsigned int unmap;
	unsigned int map;
	unsigned int in;
	unsigned int out;
	unsigned int fin;
	unsigned int fout;
	unsigned int zero;
	unsigned int segv;
	unsigned int segprot;
}procstats;

class process {
public:
	vector<vector<int>> vmas;
	int max_pte;
	vector<pte_t*> page_table;
	int procid;
	process();
	procstats* pstats;
};

process::process(){}
vector<process*> procs;

process *current_process = new process();
pte_t *current_pagetable;

typedef struct {
	process* proc;
	int vpage;
	int num;
	int select;
	int valid;
	unsigned int age;
}frame_t;

vector<frame_t*> frame_table;
list<frame_t*> free_frame;

int offset = 0;
int rnum = 0;
vector<int> randvals;

int myrandom(int framenum) {
	if (offset == rnum - 1)
		offset = 0;
	int rand = randvals[offset] % framenum;
	offset++;
	return rand;
}

class Pager {
public:
	virtual frame_t* select_victim_frame() = 0; // virtual base class
};

frame_t* Pager::select_victim_frame() {
	frame_t* f = { NULL };
	return f;
}

frame_t* allocate_frame() {
	// if there is a free frame in the frame table, return it
	if (!free_frame.empty()) {
		frame_t* f = free_frame.front();
		free_frame.pop_front();
		return f;
	}
	// else return null
	return NULL;	
}

frame_t *get_frame(Pager *THE_PAGER) {
	frame_t *frame = allocate_frame();
	if (frame == NULL) frame = THE_PAGER->select_victim_frame();
	return frame;
}

class FIFO :public Pager {
	frame_t* select_victim_frame();
};

frame_t* FIFO::select_victim_frame() {
	for (int i = 0; i < frame_table.size(); i++) {
		if (frame_table[i]->select == 0) {
			frame_t* f = frame_table[i];
			frame_table[i]->select = 1;
			return f;
		}
	}
	for (int i = 1; i < frame_table.size(); i++) {
		frame_table[i]->select = 0;
	}
	return frame_table[0];
}

class Random :public Pager {
	frame_t* select_victim_frame();
};

frame_t* Random::select_victim_frame() {
	int rand = myrandom(frame_table.size());
	return frame_table[rand];
}

class Clock :public Pager {
	frame_t* select_victim_frame();
};

frame_t* Clock::select_victim_frame() {
	for (int i = 0; i < frame_table.size(); i++) {
		if (frame_table[i]->select == 1) {
			frame_table[i]->select = 0;
			for (int j = i; j < frame_table.size(); j++) {
				if (frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED == 1) {
					frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED = 0;
				}
				else {
					if (j == frame_table.size() - 1) {
						frame_table[0]->select = 1;
					}
					else {
						frame_table[j + 1]->select = 1;
					}
					return frame_table[j];
				}
			}
			for (int j = 0; j < i; j++) {
				if (frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED == 1) {
					frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED = 0;
				}
				else {
					frame_table[j + 1]->select = 1;
					return frame_table[j];
				}
			}
			if (i == frame_table.size() - 1)
				frame_table[0]->select = 1;
			else
				frame_table[i + 1]->select = 1;
			return frame_table[i];
		}
	}
	return NULL;
}

class NRU :public Pager {
	frame_t* select_victim_frame();
};

int last_reset = 0;
int instrnum = 0;

frame_t* NRU::select_victim_frame() {
	vector<list<frame_t*>> frameclass;
	for (int i = 0; i < 3; i++) {
		list<frame_t*> fc;
		frameclass.push_back(fc);
	}
	frame_t* f = (frame_t*)malloc(sizeof(frame_t));
	f->valid = 0;
	unsigned int classnum;
	for (int i = 0; i < frame_table.size(); i++) {
		if (frame_table[i]->select == 1) {
			frame_table[i]->select = 0;
			for (int j = i; j < frame_table.size(); j++) {
				classnum = frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED * 2
					+ frame_table[j]->proc->page_table[frame_table[j]->vpage]->MODIFIED;
				if (classnum == 0) {
					f = frame_table[j];
					f->valid = 1;
					if (j == frame_table.size() - 1)
						frame_table[0]->select = 1;
					else
						frame_table[j + 1]->select = 1;
					break;
				}
				else
					frameclass[classnum - 1].push_back(frame_table[j]);
			}
			if (f->valid == 0) {
				for (int j = 0; j < i; j++) {
					classnum = frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED * 2
						+ frame_table[j]->proc->page_table[frame_table[j]->vpage]->MODIFIED;
					if (classnum == 0) {
						if (j == frame_table.size() - 1)
							frame_table[0]->select = 1;
						else
							frame_table[j + 1]->select = 1;
						f = frame_table[j];
						f->valid = 1;
						break;
					}
					else
						frameclass[classnum - 1].push_back(frame_table[j]);
				}
			}
		}
		else
			continue;
		break;
	}
	if (f->valid == 0) {
		for (int i = 0; i < 3; i++) {
			if (!frameclass[i].empty()) {
				f = frameclass[i].front();
				f->valid = 1;
				if (f->num == frame_table.size() - 1)
					frame_table[0]->select = 1;
				else
					frame_table[f->num + 1]->select = 1;
				break;
			}
		}
	}
	if (instrnum - last_reset >= 50) {
		for (int i = 0; i < frame_table.size(); i++) {
			frame_table[i]->proc->page_table[frame_table[i]->vpage]->REFERENCED = 0;
		}
		last_reset = instrnum;
	}
	return f;
}

class Aging :public Pager {
	frame_t* select_victim_frame();
};

frame_t* Aging::select_victim_frame() {
	for (int i = 0; i < frame_table.size(); i++) {
		frame_table[i]->age = frame_table[i]->age >> 1;
		unsigned int r = frame_table[i]->proc->page_table[frame_table[i]->vpage]->REFERENCED << 31;
		frame_table[i]->age += r;
		frame_table[i]->proc->page_table[frame_table[i]->vpage]->REFERENCED = 0;
	}
	for (int i = 0; i < frame_table.size(); i++) {
		if (frame_table[i]->select == 1) {
			frame_table[i]->select = 0;
			int index = i;
			for (int j = i; j < frame_table.size(); j++) {
				if (frame_table[j]->age < frame_table[index]->age) {
					index = j;
				}
			}
			for (int j = 0; j < i; j++) {
				if (frame_table[j]->age < frame_table[index]->age) {
					index = j;
				}
			}
			if (index == frame_table.size() - 1)
				frame_table[0]->select = 1;
			else
				frame_table[index + 1]->select = 1;
			return frame_table[index];
		}
	}
	return NULL;
}

class WS :public Pager {
	frame_t* select_victim_frame();
};

frame_t* WS::select_victim_frame() {
	for (int i = 0; i < frame_table.size(); i++) {
		if (frame_table[i]->select == 1) {
			frame_table[i]->select = 0;
			int index = i;
			for (int j = i; j < frame_table.size(); j++) {
				if (frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED == 1) {
					frame_table[j]->age = instrnum;
					frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED = 0;
				}
				else {
					if (instrnum - frame_table[j]->age >= 50) {
						frame_t* f = frame_table[j];
						if (j == frame_table.size() - 1)
							frame_table[0]->select = 1;
						else
							frame_table[j + 1]->select = 1;
						return f;
					}
					else {
						if (frame_table[j]->age < frame_table[index]->age) {
							index = j;
						}
					}
				}
			}
			for(int j = 0; j < i; j++) {
				if (frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED == 1) {
					frame_table[j]->age = instrnum;
					frame_table[j]->proc->page_table[frame_table[j]->vpage]->REFERENCED = 0;
				}
				else {
					if (instrnum - frame_table[j]->age >= 50) {
						frame_t* f = frame_table[j];
						if (j == frame_table.size() - 1)
							frame_table[0]->select = 1;
						else
							frame_table[j + 1]->select = 1;
						return f;
					}
					else {
						if (frame_table[j]->age < frame_table[index]->age) {
							index = j;
						}
					}
				}
			}
			if (index == frame_table.size() - 1)
				frame_table[0]->select = 1;
			else
				frame_table[index + 1]->select = 1;
			return frame_table[index];
		}
	}
	return NULL;
}

bool get_next_instruction(char *operation, int *vpage) {
	char op = *operation;
	int page = *vpage;
	if (op == 'c') {
		current_process = procs[page];
		return false;
	}
	else if (op == 'e') {
		// current process exit
		cout << "EXIT current process " << current_process->procid << endl;
		for (int j = 0; j < current_process->page_table.size(); j++) {
			//current_process->page_table[j]->filemap = 0;
			if (current_process->page_table[j]->VALID == 1) {
				for (int k = 0; k < frame_table.size(); k++) {
					if (frame_table[k]->proc!=NULL && frame_table[k]->num == current_process->page_table[j]->framenum) {
						
						cout << " UNMAP " << current_process->procid << ":" << frame_table[k]->vpage << endl;
						current_process->pstats->unmap++;
						if (current_process->page_table[j]->MODIFIED == 1 && current_process->page_table[j]->filemap == 1) {
							cout << " FOUT" << endl;
							current_process->pstats->fout++;
						}
						frame_table[k]->proc = NULL;
						frame_table[k]->vpage = 0;
						free_frame.push_back(frame_table[k]);
					}
				}
			}
			current_process->page_table[j]->VALID = 0;
			current_process->page_table[j]->PAGEDOUT = 0;
			current_process->page_table[j]->MODIFIED = 0;
			current_process->page_table[j]->REFERENCED = 0;
			
			//current_process->page_table[j]->vms = 0;
			//current_process->page_table[j]->framenum = 0;
			//current_process->page_table[j]->WRITE_PROTECTED = 0;
		}
		current_process = NULL;
		return false;
	}
	else {
		return true;
	}
}

static const char *optString = "a:f:o:";

int main(int argc, char* argv[])
{
	unsigned int mapcost = 400;
	unsigned int pagecost = 3000;
	unsigned int filecost = 2500;
	unsigned int zerocost = 150;
	unsigned int segvcost = 240;
	unsigned int segprotcost = 300;
	unsigned int accesscost = 1;
	unsigned int cscost = 121;
	unsigned int exitcost = 175;
	unsigned int cs = 0;
	unsigned int exit = 0;
	unsigned int access = 0;
	unsigned long long int totalcost = 0;

	/* read the options */
	
	char* pagername;
	char* OPFS;
	char* framecount;
	Pager* THE_PAGER;
	char* inputname = NULL;
	char* rfilename = NULL;
	int opt = 0;
	opt = getopt(argc, argv, optString);
	while (opt != -1) {
		switch (opt) {
		case 'a':
			pagername = optarg;
			break;
			
		case 'o':
			OPFS = optarg;
			break;
			
		case 'f':
			framecount = optarg;
			break;

		default:
			abort();
			break;
		}
		opt = getopt(argc, argv, optString);
	}
	int index = optind;
	inputname = argv[index];
	index++;
	rfilename = argv[index];
	string pager = pagername;
	if (pager == "f") {
		THE_PAGER = new FIFO();
	}
	else if (pager == "c") {
		THE_PAGER = new Clock();
	}
	else if (pager == "e") {
		THE_PAGER = new NRU();
	}
	else if (pager == "a") {
		THE_PAGER = new Aging();
	}
	else if (pager == "w") {
		THE_PAGER = new WS();
	}
	else if (pager == "r") {
		THE_PAGER = new Random();
		/* read the random file */
		ifstream rfile(rfilename);
		char buf[20];
		rfile.getline(buf, 20);
		rnum = atoi(buf);
		int i = 0;
		for (i = 0; i < rnum; i++) {
			rfile.getline(buf, 20);
			randvals.push_back(atoi(buf));
		}
		rfile.close();
		rfile.clear();
	}

	int ohhh = 0;
	int ptoption = 0;
	int ftoption = 0;
	int sumoption = 0;
	for (int i = 0; i < strlen(OPFS); i++) {
		if (OPFS[i] == 'O') ohhh = 1;
		else if (OPFS[i] == 'P') ptoption = 1;
		else if (OPFS[i] == 'F') ftoption = 1;
		else if (OPFS[i] == 'S') sumoption = 1;
	}
	unsigned int frame_number = 4;
	if(framecount != "")
		frame_number = atoi(framecount);

	
	/* initialize the frame table */
	for (int i = 0; i < frame_number; i++) {
		frame_t* newframe;
		newframe = (frame_t*)malloc(sizeof(frame_t));
		newframe->num = i;
		newframe->proc = NULL;
		newframe->vpage = 0;
		newframe->select = 0;
		newframe->valid = 0;
		newframe->age = 0;
		frame_table.push_back(newframe);
		free_frame.push_back(newframe);
	}
	if(pager == "c" || pager == "e" || pager == "a" || pager == "w")
		frame_table[0]->select = 1;

	/* read the input file */
	ifstream input(inputname);
	char buffer[150];
	list<char> instr;
	list<int> page;
	while (!input.eof()) {
		input.getline(buffer, 150);
		if (buffer[0] == '#') {
			continue;
		}
		int processnum = atoi(buffer);
		
		for (int i = 0; i < processnum; i++) {
			input.getline(buffer, 150);
			if (buffer[0] == '#') {
				i--;
				continue;
			}
			int vmsnum = atoi(buffer);
			process* proc = new process();
			proc->max_pte = 64;
			proc->procid = i;
			// initialize all the virtual page of the process to be a hole
			for (int k = 0; k < proc->max_pte; k++) {
				pte_t* newpte;
				newpte = (pte_t*)malloc(sizeof(pte_t));
				newpte->vms = 0;
				newpte->filemap = 0;
				newpte->framenum = 0;
				newpte->MODIFIED = 0;
				newpte->PAGEDOUT = 0;
				newpte->REFERENCED = 0;
				newpte->VALID = 0;
				newpte->WRITE_PROTECTED = 0;
				proc->page_table.push_back(newpte);
			}
			for (int j = 0; j < vmsnum; j++) {
				input.getline(buffer, 150);
				int start_vpage = atoi(strtok(buffer, " "));
				int end_vpage = atoi(strtok(NULL, " "));
				int write_protected = atoi(strtok(NULL, " "));
				int file_mapped = atoi(strtok(NULL, " "));
				/* initialize the page table entries */
				for (int p = start_vpage; p <= end_vpage; p++) {
					proc->page_table[p]->vms = 1;
					proc->page_table[p]->filemap = 0;
				}
				proc->vmas.push_back({ start_vpage, end_vpage, write_protected, file_mapped });
				proc->pstats = (procstats*)malloc(sizeof(procstats));
				proc->pstats->fin = 0;
				proc->pstats->fout = 0;
				proc->pstats->in = 0;
				proc->pstats->out = 0;
				proc->pstats->map = 0;
				proc->pstats->unmap = 0;
				proc->pstats->zero = 0;
				proc->pstats->segv = 0;
				proc->pstats->segprot = 0;
			}
			procs.push_back(proc);
		}
		break;
	}
	input.getline(buffer, 150);
	while (!input.eof()) {
		input.getline(buffer, 150);
		if (buffer[0] == '#') {
			break;
		}
		char *op = strtok(buffer, " ");
		instr.push_back(op[0]);
		page.push_back(atoi(strtok(NULL, " ")));
	}

	/* simulation */
	while (!instr.empty()) {
		char operation = instr.front();
		int vpage = page.front();
		instr.pop_front();
		page.pop_front();
		if (ohhh == 1)	cout << instrnum << ": ==> " << operation << " " << vpage << endl;
		//if (instrnum == 1999)
		//	int a = 0;
		instrnum++;
		if (operation == 'c') cs++;
		else if (operation == 'e') exit++;
		else access++;

		if (instrnum == 8)
			int a = 0;
		while (get_next_instruction(&operation, &vpage)) {
			/* handle special case of "c" and "e" instruction */
			/* now the real instructions for read and write */
			
			pte_t *pte = current_process->page_table[vpage]; //为什么这里是&
			if (pte->vms == 0) {
				// it's a hole
				if (ohhh == 1) cout << " SEGV" << endl;
				current_process->pstats->segv++;
				break;
			}
			if (!pte->VALID) {
				/* page fault */
				// this in reality will generate the page fault exception and now you execute
				// so this is done by hardware?
				frame_t *newframe = get_frame(THE_PAGER);
				current_process->page_table[vpage]->framenum = newframe->num;

				/* take care of OUT, FOUT */
				if (newframe->proc) {
					if (ohhh == 1) cout << " UNMAP " << newframe->proc->procid << ":" << newframe->vpage << endl;
					newframe->proc->page_table[newframe->vpage]->VALID = 0;
					newframe->proc->page_table[newframe->vpage]->REFERENCED = 0;
					newframe->proc->pstats->unmap++;
					if (newframe->proc->page_table[newframe->vpage]->MODIFIED == 1) {
						if (newframe->proc->page_table[newframe->vpage]->filemap == 1) {
							if (ohhh == 1) cout << " FOUT" << endl;
							newframe->proc->pstats->fout++;
						}
						else {
							if (ohhh == 1) cout << " OUT" << endl;
							newframe->proc->pstats->out++;
							newframe->proc->page_table[newframe->vpage]->PAGEDOUT = 1;
						}
					}
				}
				
				/* take care of IN, FIN and ZERO */
				if (pte->filemap == 1) {
					if (ohhh == 1) cout << " FIN" << endl;
					current_process->pstats->fin++;
					pte->PAGEDOUT = 0;
				}
				else if (pte->filemap == 2) {
					if (pte->PAGEDOUT == 1) {
						if (ohhh == 1) cout << " IN" << endl;
						current_process->pstats->in++;
					}
					else {
						if (ohhh == 1) cout << " ZERO" << endl;
						current_process->pstats->zero++;
					}
				}
				else {
					for (int vmsnum = 0; vmsnum < current_process->vmas.size(); vmsnum++) {
						if (vpage >= current_process->vmas[vmsnum][0] && vpage <= current_process->vmas[vmsnum][1]) {
							if (current_process->vmas[vmsnum][3] == 1) {
								// it's filemapped
								pte->filemap = 1;
								if (ohhh == 1) cout << " FIN" << endl;
								current_process->pstats->fin++;
							}
							if (current_process->vmas[vmsnum][2] == 1) {
								// it's write protected
								pte->WRITE_PROTECTED = 1;
							}
						}
					}
					if (pte->filemap != 1) {
						pte->filemap = 2;
						if (pte->PAGEDOUT == 1) {
							if (ohhh == 1) cout << " IN" << endl;
							current_process->pstats->in++;
						}
						else {
							if (ohhh == 1) cout << " ZERO" << endl;
							current_process->pstats->zero++;
						}
					}
				}
				
				// map this frame to the current process and current page
				newframe->proc = current_process;
				newframe->vpage = vpage;
				if (ohhh == 1) cout << " MAP " << newframe->num << endl;
				if (pager == "a")
					newframe->age = 0;
				else if (pager == "w")
					newframe->age = instrnum;
				current_process->pstats->map++;
				pte->VALID = 1;
				if (operation == 'r')
					pte->MODIFIED = 0;
				//-> figure out if/what to do with old frame if it was mapped
				// see general outline in MM-slides under Lab3 header
				// see whether and how to bring in the content of the access page
			}
			// simulate instruction execution by hardware by updating the R/M PTE bits
			// update pte(read/modify/write bits based on operations 
			pte->REFERENCED = 1;
			if (operation == 'w') {
				// write
				if (pte->WRITE_PROTECTED == 1) {
					if (ohhh == 1) cout << " SEGPROT" << endl;
					current_process->pstats->segprot++;
					
				}
				else
					pte->MODIFIED = 1;
			}
			break;
		}
	}
	/* pagetable */
	if (ptoption == 1) {
		for (int i = 0; i < procs.size(); i++) {
			cout << "PT[" << procs[i]->procid << "]: ";
			for (int j = 0; j < procs[i]->page_table.size(); j++) {
				if (procs[i]->page_table[j]->VALID == 0) {
					if (procs[i]->page_table[j]->PAGEDOUT == 1)
						cout << "# ";
					else
						cout << "* ";
				}
				else {
					cout << j << ":";
					if (procs[i]->page_table[j]->REFERENCED == 1)
						cout << "R";
					else
						cout << "-";
					if (procs[i]->page_table[j]->MODIFIED == 1)
						cout << "M";
					else
						cout << "-";
					if (procs[i]->page_table[j]->PAGEDOUT == 1)
						cout << "S ";
					else
						cout << "- ";
				}
			}
			cout << endl;
		}
	}

	/* frametable */
	if (ftoption == 1) {
		cout << "FT: ";
		for (int j = 0; j < frame_table.size(); j++) {
			if (frame_table[j]->proc != NULL) {
				cout << frame_table[j]->proc->procid << ":" << frame_table[j]->vpage << " ";
			}
			else
				cout << "* ";
		}
		cout << endl;
	}
	
	/* summary statistics of each process */
	if (sumoption == 1) {
		for (int i = 0; i < procs.size(); i++) {
			cout << "PROC[" << procs[i]->procid << "]: U=" << procs[i]->pstats->unmap << " M=" << procs[i]->pstats->map
				<< " I=" << procs[i]->pstats->in << " O=" << procs[i]->pstats->out << " FI=" << procs[i]->pstats->fin
				<< " FO=" << procs[i]->pstats->fout << " Z=" << procs[i]->pstats->zero << " SV=" << procs[i]->pstats->segv
				<< " SP=" << procs[i]->pstats->segprot << endl;
			totalcost += (procs[i]->pstats->unmap + procs[i]->pstats->map)*mapcost
				+ (procs[i]->pstats->fin + procs[i]->pstats->fout)*filecost
				+ (procs[i]->pstats->in + procs[i]->pstats->out)*pagecost
				+ procs[i]->pstats->zero*zerocost
				+ procs[i]->pstats->segv*segvcost
				+ procs[i]->pstats->segprot*segprotcost;
		}

		/* total cost */
		totalcost += cs * cscost + exit * exitcost + access * accesscost;
		cout << "TOTALCOST " << instrnum << " " << cs << " " << exit << " " << totalcost << endl;
	}
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
