#pragma once
#include <stdio.h>

class tester {
public:
	int id = 0;
	void process() {
		printf("tester %d have been catched\n", id);
	}
	tester(int i = 0) :id(i) {};
};