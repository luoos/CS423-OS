import matplotlib.pyplot as plt

def read_raw_data(filename):
    data = []
    with open(filename, 'r') as f:
        for line in f:
            r = line.split()
            data.append(list(map(lambda v: int(v), r)))
    return data

def acc_data(data):
    t0 = data[0][0]
    cur = data[0][:]
    new_data = [[0] + cur[1:]]
    for t, a, b, c in data[1:]:
        if a + b + c == 0:
            continue
        cur[0] = t - t0
        cur[1] += a
        cur[2] += b
        cur[3] += c
        new_data.append(cur[:])
    return new_data

def case_1(filename, title, description):
    raw_data = read_raw_data(filename)
    scanned_data = acc_data(raw_data)
    X = [d[0] for d in scanned_data]
    Y = [d[1] for d in scanned_data]

    plt.scatter(X, Y)
    plt.title(title + '\n' + description)
    plt.xlabel('Time/jiffies')
    plt.ylabel('Page Fault')
    plt.show()

def get_cpu_util(filename):
    raw_data = read_raw_data(filename)
    scanned_data = acc_data(raw_data)
    total_time = scanned_data[-1][0] - scanned_data[0][0]
    cpu_time = scanned_data[-1][-1]
    return cpu_time / total_time

def get_N(filename):
    return int(filename.split('-')[1].split('.')[0])

def get_X(file_list):
    return [get_N(name) for name in file_list]

def case_2(file_list):
    Y = [get_cpu_util(file) for file in file_list]
    X = get_X(file_list)

    plt.scatter(X, Y)
    plt.title('Case 2: Multiprogramming')
    plt.xlabel('N (number of process-5)')
    plt.ylabel('CPU utilization/%')
    plt.show()


if __name__ == "__main__":
    case_1("profile1-1.data",
            "Case 1-1: Thrashing and Locality",
            "nice ./work 1024 R 50000 & nice ./work 1024 R 10000");
    case_1("profile1-2.data",
            "Case 1-2: Thrashing and Locality",
            "nice ./work 1024 R 50000 & nice ./work 1024 L 10000");
    case_2_files = ['profile2-1.data', 'profile2-5.data',
            'profile2-11.data', 'profile2-15.data',
            'profile2-19.data', 'profile2-20.data']
    case_2(case_2_files)