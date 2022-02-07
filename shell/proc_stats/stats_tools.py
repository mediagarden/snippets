import paramiko
import re


class StatsUtils:
    def __init__(self):
        self.username = 'firefly'
        self.password = 'firefly'
        self.port = 22
        self.proc = "AudioPlayer"
        self.file_name = "calc_cpu_rate.sh"
        self.up_load_path = "/home/firefly/" + self.file_name
        pass

    def __del__(self):
        pass

    def get_pid(self, device):
        ##1.创建一个ssh对象
        client = paramiko.SSHClient()

        # 2.解决问题:如果之前没有，连接过的ip，会出现选择yes或者no的操作，
        ##自动选择yes
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        # 3.连接服务器
        client.connect(hostname=device,
                       port=self.port,
                       username=self.username,
                       password=self.password)
        # 4.执行操作
        stdin, stdout, stderr = client.exec_command('ps -aux |grep ' + self.proc)

        # 5.获取命令执行的结果
        result = stdout.read().decode('utf-8')
        result = result.splitlines()
        pid = -1
        for line in result:
            if line.find("bash") == -1 and line.find("grep") == -1:
                pid = int(re.search(r'\d+', line).group())
        # 6.关闭连接
        client.close()
        return pid

    def get_sys_mem(self, device):
        ##1.创建一个ssh对象
        client = paramiko.SSHClient()

        # 2.解决问题:如果之前没有，连接过的ip，会出现选择yes或者no的操作，
        ##自动选择yes
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        # 3.连接服务器
        client.connect(hostname=device,
                       port=self.port,
                       username=self.username,
                       password=self.password)
        # 4.执行操作
        stdin, stdout, stderr = client.exec_command('free -m')

        # 5.获取命令执行的结果
        result = stdout.read().decode('utf-8')
        result = result.splitlines()
        mem_total = -1
        mem_used = -1
        for line in result:
            if line.find("Mem") != -1:
                int_array = re.findall(r'\d+', line)
                mem_total = int(int_array[0])
                mem_used = int(int_array[1])
        # 6.关闭连接
        client.close()
        return mem_total, mem_used

    def upload_sh(self, device):
        trans = paramiko.Transport(
            sock=(device, self.port)
        )

        trans.connect(
            username=self.username,
            password=self.password
        )

        sftp = paramiko.SFTPClient.from_transport(trans)
        sftp.put(self.file_name, self.up_load_path)
        sftp.close()

    def get_cpu_rate(self, device):
        ##1.创建一个ssh对象
        client = paramiko.SSHClient()

        # 2.解决问题:如果之前没有，连接过的ip，会出现选择yes或者no的操作，
        ##自动选择yes
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        # 3.连接服务器
        client.connect(hostname=device,
                       port=self.port,
                       username=self.username,
                       password=self.password)
        # 4.执行操作
        stdin, stdout, stderr = client.exec_command('chmod +x ' + self.up_load_path)

        # 4.执行操作
        stdin, stdout, stderr = client.exec_command('bash ' + self.up_load_path)

        # 5.获取命令执行的结果
        result = stdout.read().decode('utf-8')
        result = result.splitlines()

        cpus = []
        average = []

        for line in result:
            if line.find("Average") == -1:
                values = line.split(':')
                cpu = int(re.search(r'\d+', values[1]).group())
                cpus.append(cpu)
            else:
                values = line.split(':')
                average = int(re.search(r'\d+', values[1]).group())

        # 6.关闭连接
        client.close()
        return cpus, average

    def get_stats_info(self, device):
        stats_info = {}
        pid = self.get_pid(device)
        mem_total, mem_used = self.get_sys_mem(device)
        self.upload_sh(device)
        cpus, average = self.get_cpu_rate(device)
        stats_info["pid"] = pid
        stats_info["mem_total"] = mem_total
        stats_info["mem_used"] = mem_used
        stats_info["cpus"] = cpus
        stats_info["cpu_average"] = average
        # print(stats_info)
        return stats_info
