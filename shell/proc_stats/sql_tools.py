#!/usr/bin/python
import json
import sqlite3
import os
import time


class SqlUtils:
    def __init__(self):
        self.conn = sqlite3.connect('device.db')

    def __del__(self):
        self.conn.close()

    def insert_device(self, ip, name):
        c = self.conn.cursor()
        cmd = "INSERT INTO device (ip,name) VALUES ('{}','{}')".format(ip, name)
        # print(cmd)
        cursor = c.execute(cmd)
        self.conn.commit()

    def update_device(self, ip, name):
        c = self.conn.cursor()
        cmd = "UPDATE device set name='{}' WHERE ip ='{}'".format(name, ip)
        # print(cmd)
        cursor = c.execute(cmd)
        self.conn.commit()

    def get_device(self, ip):
        c = self.conn.cursor()
        cmd = "SELECT ip, name from device WHERE ip ='{}'".format(ip)
        # print(cmd)
        cursor = c.execute(cmd)
        for row in cursor:
            return {"ip": row[0], "name": row[1]}
        return None

    def insert_stats(self, ip, stats):
        stats_info = json.dumps(stats)
        c = self.conn.cursor()
        cmd = "INSERT INTO stats (ip,stats,stamp) VALUES ('{}','{}','{}')".format(ip, stats_info, int(time.time()))
        # print(cmd)
        cursor = c.execute(cmd)
        self.conn.commit()

    def get_stats(self, ip):
        c = self.conn.cursor()
        cmd = "SELECT * from stats WHERE ip ='{}' order by id desc LIMIT 1".format(ip)
        # print(cmd)
        cursor = c.execute(cmd)
        for row in cursor:
            stats = row[2]
            return json.loads(stats)
        return None
