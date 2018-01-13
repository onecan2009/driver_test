#!/bin/bash


insmod ./globalfifo.ko

mknod /dev/globalfifo c 250 0
