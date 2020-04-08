app.out : brisk_knn_intellino.o intellino_spi.o
	g++ -o app.out brisk_knn_intellino.o intellino_spi.o

brisk_knn_intellino.o : brisk_knn_intellino.cpp
	g++ -c -o brisk_knn_intellino.o brisk_knn_intellino.cpp

intellino_spi.o : intellino_spi.cpp
	g++ -c -o intellino_spi.o intellino_spi.cpp 

clean :
	rm -f *.o
	rm -f app.out
