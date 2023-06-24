#!/usr/bin/env python3

import argparse

def GREEN(string):
    return '\033[1;32m' + string + '\033[0m'

def RED(string):
    return '\033[1;31m' + string + '\033[0m'

def YELLOW(string):
    return '\033[1;33m' + string + '\033[0m'

def verify_fib(max_k=500):

    expect = [0, 1]
    result = []
    dics = {}

    for i in range(2, max_k + 1):
        expect.append(expect[i - 1] + expect[i - 2])
    with open('out', 'r') as f:
        result = f.read().split('\n')
    for line in result[:-1]:
        k, fib = line.split()
        dics[int(k)] = int(fib)
    for key, value in dics.items():
        if value != expect[key]:
            print(RED(' Fib(%s) Failed' %(key)))
            print(YELLOW(' Input: %s' %(value)))
            print(YELLOW(' Expected: %s' %(expect[key])))
            exit()
    
    print(GREEN(' Passed [-]'))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-k', '--max_k', type=int, default=500)
    args = parser.parse_args()
    verify_fib(args.max_k)

if __name__ == '__main__':
    main()
