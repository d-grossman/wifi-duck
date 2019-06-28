#!/usr/bin/env python
import sys
from time import sleep

if __name__ == "__main__":
  a = open(sys.argv[1],'r')
  lines = a.readlines()

  for line  in lines:
    for c in line:
      print(c,end="")
      sys.stdout.flush()
      sleep(0.01)
    sleep(0.5)
  a.close()
