import json
import sqlite3
import re
import sql_tools
import stats_tools

device_list_file = "device_list.txt"


def trim(s):
    r = re.findall('[\S]+', s)
    return " ".join(r)


# 读取设备列表
def read_device_list():
    device_list = []
    with open(device_list_file, 'r', encoding='utf-8') as file:
        device_lines = file.readlines()
    for line in device_lines:
        lines = line.split(" ")
        device = {"ip": trim(lines[0]), "name": trim(lines[1])}
        device_list.append(device)
    return device_list


# 更新设备列表
def update_device_list(device_list):
    for device in device_list:
        sql = sql_tools.SqlUtils()
        if sql.get_device(device["ip"]) is None:
            sql.insert_device(device["ip"], device["name"])
        else:
            sql.update_device(device["ip"], device["name"])
        # print(sql.get_device(device["ip"]))
    pass


# 更新统计信息
def update_device_stats(ip, stats_info):
    sql = sql_tools.SqlUtils()
    sql.insert_stats(ip, stats_info)


# 获取统计信息
def get_device_stats(ip):
    sql = sql_tools.SqlUtils()
    stats_info = sql.get_stats(ip)
    return stats_info


def cmp_stat_report(ip, name, stats_info, last_stats_info):
    print("进程号:{}".format(stats_info["pid"]))
    print("内存使用率:{}%".format(int((stats_info["mem_used"] / stats_info["mem_total"]) * 100)))
    print("cpu使用率:{}%,平均使用率:{}%".format(stats_info["cpus"], stats_info["cpu_average"]))
    if last_stats_info is not None:
        if stats_info["pid"] != last_stats_info["pid"]:
            print("进程号不一致:{}->{}".format(last_stats_info["pid"], stats_info["pid"]))

    if int((stats_info["mem_used"] / stats_info["mem_total"]) * 100) > 50:
        print("内存使用率过高:{}%".format(int((stats_info["mem_used"] / stats_info["mem_total"]) * 100)))

    cpu_id = 0
    for cpu in stats_info["cpus"]:
        if cpu > 80:
            print("CPU{}使用率过高:{}%".format(cpu_id, cpu))
        cpu_id += 1
    if stats_info["cpu_average"] > 50:
        print("CPU平均使用率过高:{}%".format(stats_info["cpu_average"]))


# 生成统计报告
def gen_stats_report(ip, name):
    print("设备名称:", name)
    print("设备IP:", ip)
    try:
        stats_utils = stats_tools.StatsUtils()
        stats_info = stats_utils.get_stats_info(ip)
        last_stats_info = get_device_stats(ip)
        cmp_stat_report(ip, name, stats_info, last_stats_info)
        update_device_stats(ip, stats_info)
    except:
        print('状态:获取统计信息失败')


def main():
    device_list = read_device_list()
    update_device_list(device_list)

    for device in device_list:
        ip = device["ip"]
        name = device["name"]
        gen_stats_report(ip, name)


if __name__ == '__main__':
    main()
    pass
