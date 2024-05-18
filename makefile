main:main.cc
	g++ -o gobang $^ -std=c++11 -lmysqlclient -ljsoncpp -lpthread -lboost_system
.PHONY:clean
clean:
	rm -f gobang DEG_log.txt ERR_log.txt nohup.out