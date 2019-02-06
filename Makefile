mmu:mmu.o
	g++ mmu.o -o mmu

mmu.o:mmu.cpp
	g++ -std=c++11 -O2 mmu.cpp -c -o mmu.o

clean:
	rm mmu
	rm *.o
