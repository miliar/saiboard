import json
import redis
import numpy as np
from game_record import GameRecord


class Play:
    def __init__(self, redis_host, redis_port):
        self.players = {"B": "Human", "W": "Human"}
        self.game = None
        self.display_mode = ""
        self.all_display_modes = [
            "",
            "top_ai_moves",
            "next_moves",
            "ownership",
        ]
        self.last_board_state = None
        self.invalid_board = False
        self.redis_conn = redis.Redis(host=redis_host, port=redis_port)
        self.pubsub = self.redis_conn.pubsub()
        self.pubsub.subscribe("board_out")
        self.pubsub.subscribe("katago_out")
        self.pubsub.subscribe("outside")
        self.colors = {
            "B": (0, 150, 150, 0),
            "W": (0, 0, 0, 150),
            "REMOVE": (250, 0, 0, 0),
            "AI": (0, 0, 250, 150),
            "RANK1": (0, 250, 0, 0),
            "RANK2": (70, 180, 0, 0),
            "RANK3": (125, 125, 0, 0),
            "RANK4": (180, 70, 0, 0),
            "RANK5": (250, 0, 0, 0),
        }

    def start_game(self):
        while True:
            self._setup_new_game()
            while self.game:
                message = self.pubsub.get_message()
                if not message:
                    continue
                if message["type"] != "message":
                    continue
                self._handle_new_message(
                    payload=json.loads(message["data"]),
                    channel=message["channel"].decode(),
                )

    def _setup_new_game(self):
        self._communicate(
            f"New game {self.players['B']} (black) vs {self.players['W']}(white)"
        )
        self.redis_conn.publish("board_in", json.dumps({"name": "all_leds_off"}))
        self.invalid_board = False
        self.game = GameRecord()
        request = self.game.start()
        self.redis_conn.publish("katago_in", request)

    def _handle_new_message(self, payload, channel):
        if channel == "board_out":
            self._handle_payload_board_out(payload)
            return
        if channel == "katago_out":
            self._handle_payload_katago_out(payload)
            return
        if channel == "outside":
            self._handle_payload_outside(payload)
            return

    def _handle_payload_board_out(self, payload):
        if boot_message := payload.get("boot"):
            self._communicate(boot_message)
        new_board_state = payload.get("new_board_state")
        if new_board_state is None:
            return
        self.last_board_state = {tuple(stone) for stone in new_board_state}
        self._handle_new_board_state()

    def _communicate(self, message, channel="message"):
        print(message)
        self.redis_conn.publish("game", json.dumps({channel: message}, default=list))

    def _handle_new_board_state(self):
        self.redis_conn.publish("board_in", json.dumps({"name": "all_leds_off"}))
        try:
            self._record_move()
        except RuntimeError as e:
            self._communicate(str(e), channel="error")
            self._display_invalid_board()
            self.invalid_board = True
            return
        self._print_board()
        self._communicate(self.game.current_node.data, channel="current_node")
        self._communicate(self.game.graph_data, channel="graph")
        self._display_valid_board()

    def _record_move(self):
        if self.last_board_state == self.game.last_board_state:
            self.invalid_board = False
            self._communicate("resolved", channel="error")
            return
        if self.invalid_board:
            raise RuntimeError("Board does not match game record")
        if added_stones := (self.last_board_state - self.game.last_board_state):
            try:
                self._add_move(added_stones)
            except RuntimeError:
                if removed := (self.game.last_board_state - self.last_board_state):
                    self._undo_stones(removed)
                    self._add_move(added_stones)

        else:
            self._undo_stones(self.game.last_board_state - self.last_board_state)
        if self.last_board_state != self.game.last_board_state:
            raise RuntimeError("Board does not match game record")

    def _add_move(self, added_stones):
        if (len(added_stones) > 2) or (
            len(added_stones) == 2 and (len(set([x[0] for x in added_stones])) == 1)
        ):
            raise RuntimeError(f"Too many stones! Please remove {added_stones}")
        if self.players[self.game.current_player] == "AI":
            if self.game.next_ai_move not in [
                list(stone) for stone in list(added_stones)
            ]:
                self._communicate(f"Wrong move, Human! Please remove {added_stones}")
        if (len(added_stones) == 1) and next(iter(added_stones))[
            0
        ] != self.game.current_player:
            raise RuntimeError(f"Wrong player! Please remove {added_stones}")
        for stone in sorted(
            list(added_stones),
            key=lambda x: x[0] == self.game.current_player,
            reverse=True,
        ):
            request = self.game.record_move(stone)
            if request:
                self.redis_conn.publish("katago_in", request)

    def _undo_stones(self, removed_stones):
        self.game.undo_stones(removed_stones)

    def _display_invalid_board(self):
        stones_to_add = self.game.last_board_state - self.last_board_state
        stones_to_remove = self.last_board_state - self.game.last_board_state
        leds = [[stone[1], *self.colors[stone[0]]] for stone in stones_to_add] + [
            [stone[1], *self.colors["REMOVE"]] for stone in stones_to_remove
        ]
        self.redis_conn.publish("board_in", json.dumps({"name": "led", "leds": leds}))

    def _print_board(self):
        column = "ABCDEFGHJKLMNOPQRST"
        result = []
        for row in range(
            19,
            0,
            -1,
        ):
            result.append(
                "%2d " % (row)
                + " ".join(self._format_pt(f"{col}{row}") for col in column)
            )
        result.append("    " + "  ".join(i for i in column))
        print("\n".join(result))

    def _format_pt(self, pt):
        if ("B", pt) in self.last_board_state:
            return " #"
        if ("W", pt) in self.last_board_state:
            return " o"
        return " ."

    def _display_valid_board(self):
        if self.display_mode == "top_ai_moves":
            self._display_top_ai_moves()
            return
        if self.display_mode == "next_moves":
            self._display_next_moves()
            return
        if self.display_mode == "ownership":
            self._display_ownership()
            return
        if self.players[self.game.current_player] == "AI":
            self._display_next_ai_move()
            return

    def _display_top_ai_moves(self, max_moves=50):
        if moves := self.game.top_ai_moves:
            leds = [
                [move["move"], *self._color_for_score_change(move["score_change"])]
                for move in moves[:max_moves]
                if move["move"] != "pass"
            ]
            self.redis_conn.publish(
                "board_in", json.dumps({"name": "led", "leds": leds})
            )

    def _color_for_score_change(self, score_change):
        if score_change >= 0:
            return self.colors["RANK1"]
        if score_change > -1.5:
            return self.colors["RANK2"]
        if score_change > -2.5:
            return self.colors["RANK3"]
        if score_change > -3.5:
            return self.colors["RANK4"]
        return self.colors["RANK5"]

    def _display_next_moves(self):
        if moves := self.game.next_moves:
            self.redis_conn.publish(
                "board_in",
                json.dumps(
                    {
                        "name": "led",
                        "leds": [
                            [move[1], *self.colors[move[0]]]
                            for move in moves
                            if move[1] != "pass"
                        ],
                    }
                ),
            )

    def _display_ownership(self):
        if moves := self.game.ownership:
            self.redis_conn.publish(
                "board_in",
                json.dumps(
                    {
                        "name": "led",
                        "leds": [
                            [
                                move[0],
                                *tuple(
                                    [
                                        int(n)
                                        for n in np.array(
                                            move[1][1]
                                            * np.array(self.colors[move[1][0]]),
                                            dtype=int,
                                        )
                                    ]
                                ),
                            ]
                            for move in moves.items()
                            if (
                                (move[1][1] > 0.2)
                                and (move[1][0], move[0]) not in self.last_board_state
                            )
                        ],
                    }
                ),
            )

    def _display_next_ai_move(self):
        if not self.game.next_ai_move:
            return
        if self.game.next_ai_move[1] == "pass":
            self._record_pass()
            return
        self.redis_conn.publish(
            "board_in",
            json.dumps(
                {
                    "name": "led",
                    "leds": [[self.game.next_ai_move[1], *self.colors["AI"]]],
                }
            ),
        )

    def _record_pass(self):
        self.redis_conn.publish("board_in", json.dumps({"name": "all_leds_off"}))
        self._communicate(f"{self.game.current_player} passed")
        request = self.game.record_move((self.game.current_player, "pass"))
        if request:
            self.redis_conn.publish("katago_in", request)
        self._communicate(self.game.current_node.data, channel="current_node")
        self._communicate(self.game.graph_data, channel="graph")
        self._display_valid_board()

    def _handle_payload_katago_out(self, payload):
        try:
            self.game.set_analysis(payload)
            self._communicate(self.game.graph_data, channel="graph")
            self._display_valid_board()
            if self.game.final_score:
                self._communicate(
                    f"Final score: {self.game.final_score}", channel="error"
                )
                # Hack for review mode
                self.players = {"B": "Human", "W": "Human"}
                self.game.final_score = ""
                self.game.game_over = False
        except RuntimeError as e:
            self._communicate(str(e), channel="error")
            self._display_invalid_board()
            self.invalid_board = True

    def _handle_payload_outside(self, payload):
        print(f"Requested from outside: {payload}")
        if config := payload.get("new_game"):
            self.players["B"], self.players["W"] = config.get("player_b"), config.get(
                "player_w"
            )
            self.game = None
            return
        if (display_mode := payload.get("display_mode")) in self.all_display_modes:
            self._set_new_display_mode(display_mode)
            return
        if current_nid := payload.get("current_nid"):
            self.game.current_nid = current_nid
            self.invalid_board = True
            self._handle_new_board_state()
            return
        if payload.get("pass"):
            self._record_pass()
            return
        if payload.get("refresh_data"):
            self._communicate_states()
            return

    def _set_new_display_mode(self, display_mode):
        self.display_mode = display_mode
        self._communicate(f"New display mode: {self.display_mode}", channel="debug")
        if not self.invalid_board:
            self.redis_conn.publish("board_in", json.dumps({"name": "all_leds_off"}))
            self._display_valid_board()

    def _communicate_states(self):
        if self.game.graph_data:
            self._communicate(self.game.graph_data, channel="graph")
        if self.game.current_node.data:
            self._communicate(self.game.current_node.data, channel="current_node")
        self._communicate(
            {
                "players": self.players,
                "display_mode": [
                    self.display_mode == mode for mode in self.all_display_modes[1:]
                ],
            },
            channel="button_states",
        )
