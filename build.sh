cd serial/
make clean
make
mv build/libserial.a ../libserial.a
cd ..
g++ main.cpp libserial.a -lpthread -Iincludes