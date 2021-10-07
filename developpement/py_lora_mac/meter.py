class PhyMeter:

    instance = None

    def __init__(self, dest_file):
        self.data_list=[]#[[data, put_time, send_time, diff]]
        self.counter = -1
        self.dest_file = dest_file

    def put(self,data):
        data.stat_id = self._get_id()
        self.data_list.append([data, time.perf_counter_ns(), -1, -1])

    def send(self,data):
        r = self.data_list[data.stat_id]
        r[2]=time.perf_counter_ns()
        r[3]=r[2]-r[1]

    @staticmethod
    def getMeter(dest_file='stat.txt'):
        if PhyMeter.instance == None:
            PhyMeter.instance = PhyMeter(dest_file)
        return PhyMeter.instance

    def _get_id(self):
        self.counter+=1
        return self.counter

    def export_data(self):
        #with open(self.dest_file, "w", newline='') as f:
        writer = csv.writer(open(self.dest_file, "w", newline=''), 'unix')
        writer.writerow(['put time', 'send time', 'delta', 'data'])
        writer.writerow([self.data_list[0][0], self.data_list[-1][1], len(self.data_list), 'general info'])
        for data in self.data_list:
            writer.writerow([data[1], data[2], data[3], str(data[0])])
#if self._last_sended.cmd == UartCommand.TX:
#    log.info("STAT: PHY TX: %d", time.monotonic_ns())
#if TEST_LOSS:
#    if not random.random() < LOSS_PROBABILITY:
#        log.warning("LOSS TEST: don't send %s", str(loraFrame))
#        return