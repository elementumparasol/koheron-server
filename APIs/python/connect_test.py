import sys

from koheron_tcp_client import KClient, command

class Tests:
	def __init__(self, client):
		self.client = client

	@command('TESTS')
	def set_float(self, f):
		return client.recv_int(4)

	@command('TESTS')
	def send_std_array(self):
		return client.recv_buffer(10, data_type='float32')


if __name__ == "__main__":
	toks = sys.argv[1].split('=')
	assert(toks[0] == '--port')
	port = int(toks[1])

	client = KClient('127.0.0.1', port, verbose=True)
	client.get_stats()

	tests = Tests(client)
	print tests.set_float(12.54)
	print tests.send_std_array()