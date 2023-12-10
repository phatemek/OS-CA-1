all: source main house bill

source: source.cpp
	g++ -std=c++17 source.cpp -o source.out csv.h -lpthread

main: main.cpp
	g++ --std=c++17 -o main.out main.cpp

house: house.cpp
	g++ --std=c++17 -o house.out house.cpp

bill: bill.cpp
	g++ --std=c++17 -o bill.out bill.cpp csv.h -lpthread

clean:
	rm -f source.out main.out house.out bill.out