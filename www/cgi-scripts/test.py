#!/usr/bin/python3
print("Content-Type: text/plain\r\n\r\n", end="")
# 循环吐出超过 64KB 的数据
for i in range(10000):
    print("This is a lot of data to fill the pipe buffer...")