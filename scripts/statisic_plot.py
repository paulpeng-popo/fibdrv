import os
import argparse
import numpy as np
import subprocess


def outlier_filter(datas, threshold = 2):
    datas = np.array(datas)
    z = np.abs((datas - datas.mean()) / datas.std())
    return datas[z < threshold]


def data_processing(data_set, n):
    catgories = data_set[0].shape[0]
    samples = data_set[0].shape[1]
    final = np.zeros((catgories, samples))

    for c in range(catgories):        
        for s in range(1, samples):
            final[c][0] = data_set[0][c][0]
            final[c][s] =                                                    \
                outlier_filter([data_set[i][c][s] for i in range(n)]).mean()
            
    return final


def main():
    current_path = os.path.dirname(os.path.abspath(__file__))
    temp_file = os.path.join(current_path, "temp.txt")
    parent_path = os.path.dirname(current_path)

    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--number", type=int, default=50)
    parser.add_argument("-o", "--output", type=str, default="data.txt")
    parser.add_argument("-cpu", "--cpu", type=int, default=5)
    args = parser.parse_args()

    data_set = []
    for _ in range(args.number):
        subprocess.call("sudo taskset -c {} {} > {}".format(args.cpu, 
            os.path.join(parent_path, "client"), temp_file), shell=True)
        data = np.loadtxt(temp_file, dtype=np.int32)
        data_set.append(data)

    # delete temp file
    os.remove(temp_file)

    final = data_processing(data_set, args.number)
    np.savetxt(os.path.join(current_path, args.output), final, fmt="%d")


if __name__ == "__main__":
    main()
