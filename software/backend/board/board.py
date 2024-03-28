import json
import numpy as np
from collections import deque
import socket
import redis
from retry import retry
import logging

logging.basicConfig()


class Board:
    def __init__(
        self,
        host,
        port,
        redis_host,
        redis_port,
        queue_out,
        queue_in,
        board_size,
        threshold_white,
        threshold_black,
        threshold_touch,
        nr_of_boot_up_rounds,
        state_cache_len,
        socket_timeout,
        touch_correct_factor,
        data_end_marker,
    ):
        self.redis_conn = redis.Redis(host=redis_host, port=redis_port)
        self.host = host
        self.port = port
        self.queue_out = queue_out
        self.queue_in = queue_in
        self.pubsub = None
        self.socket = None
        self.board_size = board_size
        self.threshold_white = threshold_white
        self.threshold_black = threshold_black
        self.threshold_touch = threshold_touch
        self.nr_of_boot_up_rounds = nr_of_boot_up_rounds
        self.hall_init = np.zeros((self.board_size, self.board_size))
        self.touch_init = 0
        self.last_state = set()
        self.state_cache_len = state_cache_len
        self.state_cache = deque(
            [{}] * self.state_cache_len, maxlen=self.state_cache_len
        )
        self.led_values = dict()
        self.socket_timeout = socket_timeout
        self.touch_correct_factor = touch_correct_factor
        self.data_end_marker = data_end_marker

    @property
    def glowing_leds(self):
        return {pos: color for pos, color in self.led_values.items() if sum(color) > 0}

    def run(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as self.socket:
            self._boot_up()
            while True:
                self._board_loop()

    def _boot_up(self):
        self._publish({"boot": "boot start"})
        self._boot_up_setup()
        self._boot_up_leds()
        self._boot_up_hall()
        self._publish({"boot": "boot done"})

    def _boot_up_setup(self):
        self._publish({"boot": "boot setup"})
        self.socket.settimeout(self.socket_timeout)
        self.pubsub = self.redis_conn.pubsub()
        self.pubsub.subscribe(self.queue_in)
        self.socket.connect((self.host, self.port))

    def _boot_up_leds(self):
        self._publish({"boot": "led init"})
        self._request_payload(
            json.dumps(
                {
                    "name": "led",
                    "leds": [
                        [i, k, 0, 0, 0, 0]
                        for i in range(self.board_size)
                        for k in range(self.board_size)
                    ],
                }
            )
        )

    def _boot_up_hall(self):
        self._publish({"boot": "hall init"})
        for _ in range(self.nr_of_boot_up_rounds):
            hall, touch = self._sensor_data()
            self.hall_init += hall
            self.touch_init += touch
        self.hall_init = np.rint(self.hall_init / self.nr_of_boot_up_rounds)
        self.touch_init = np.rint(self.touch_init / self.nr_of_boot_up_rounds)

    def _publish(self, message):
        print(message)
        self.redis_conn.publish(self.queue_out, json.dumps(message))

    @retry(ConnectionResetError, tries=6, delay=2, backoff=2)
    def _request_payload(self, payload):
        try:
            return self._send_request(payload)
        except ConnectionResetError:
            print("ConnectionResetError: Reconnecting...")
            self.socket.connect((self.host, self.port))
            raise

    @retry(socket.timeout, tries=3, delay=2)
    def _send_request(self, payload):
        self.socket.sendall(payload.encode())
        data = b""
        while not data.endswith(self.data_end_marker):
            chunk = self.socket.recv(4096)
            if not chunk:
                break
            data += chunk
        return data.decode()[:-1]

    def _sensor_data(self):
        sensor_data = json.loads(self._request_payload(json.dumps({"name": "hall"})))
        return np.around(np.array(sensor_data.get("hall"))), sensor_data.get("touch")

    def _board_loop(self):
        if message := self.pubsub.get_message():
            if message["type"] != "message":
                # type: One of the following: ‘subscribe’, ‘unsubscribe’, ‘psubscribe’, ‘punsubscribe’, ‘message’, ‘pmessage’
                # https://redis-py.readthedocs.io/en/stable/advanced_features.html#publish-subscribe
                return
            payload = json.loads(message["data"])
            print(payload)
            self._handle_request(payload)
        else:
            self._wait_for_move()

    def _handle_request(self, payload):
        endpoint = payload.get("name")
        if endpoint == "led":
            self._handle_request_led(payload)
            return
        if endpoint == "all_leds_off":
            self._handle_request_led_off()
            return
        if endpoint == "led_state":
            self._publish({"led_state": self.glowing_leds})
            return
        if endpoint == "board_state":
            self._publish({"board_state": list(self.last_state)})
            return
        self._publish({"error": f"Endpoint <{endpoint}> not implemented"})

    def _handle_request_led(self, payload):
        if not payload.get("leds"):
            self._publish({"error": "Missing payload <leds>"})
            return
        leds = [
            [*self._position_to_row_col(led[0]), led[1], led[2], led[3], led[4]]
            for led in payload.get("leds")
        ]
        data = self._request_payload(json.dumps({"name": "led", "leds": leds}))
        self._publish(json.loads(data))
        for led in payload.get("leds"):
            self.led_values[led[0]] = (led[1], led[2], led[3], led[4])

    def _handle_request_led_off(self):
        if not self.glowing_leds:
            self._publish({"status": "No leds to turn off"})
            return
        leds = [
            [*self._position_to_row_col(led), 0, 0, 0, 0] for led in self.glowing_leds
        ]
        data = self._request_payload(json.dumps({"name": "led", "leds": leds}))
        self._publish(json.loads(data))
        self.led_values = dict()

    def _position_to_row_col(self, move):
        row = list(range(self.board_size, 0, -1)).index(int(move[1:]))
        col = "ABCDEFGHJKLMNOPQRST".find(move[0])
        return row, col

    def _wait_for_move(self):
        hall, touch = self._sensor_data()
        hall = hall - self.hall_init
        touch = self._correct_touch_sensor_data(touch)
        if touch < self.threshold_touch:
            self._look_for_move(hall)
        else:
            print(f"Touch {touch} treshold {self.threshold_touch}")

    def _correct_touch_sensor_data(self, touch):
        # more stones, more capacity
        return (
            touch
            - self.touch_init
            - int(self.touch_correct_factor * len(self.last_state))
        )

    def _look_for_move(self, hall):
        state = self._get_state(hall)
        if state == self.last_state:
            return
        self._handle_new_state(state)

    def _get_state(self, hall):
        return self._state_to_gtp(
            (hall > self.threshold_white).astype(int)
            - (hall < self.threshold_black).astype(int)
        )

    def _state_to_gtp(self, state):
        return {
            self._format_to_gtp(color, row, col)
            for row in range(self.board_size)
            for col in range(self.board_size)
            if (color := state[row][col]) != 0
        }

    def _format_to_gtp(self, color, row, col):
        color = "B" if color == -1 else "W"
        return color, self._row_col_to_position(row, col)

    def _row_col_to_position(self, row, col):
        return f'{"ABCDEFGHJKLMNOPQRST"[col]}{list(range(self.board_size,0,-1))[row]}'

    def _handle_new_state(self, state):
        self.state_cache.appendleft(state)
        if all(x == self.state_cache[0] for x in self.state_cache):
            self._publish(
                {
                    "new_board_state": list(state),
                    "added": list(state - self.last_state),
                    "removed": list(self.last_state - state),
                }
            )
            self.last_state = state
