from treelib import Tree
from treelib.exceptions import NodeIDAbsentError
import json


class GameRecord:
    def __init__(self):
        self.moves = Tree()
        self.last_board_state = None
        self.prisoners = dict()
        self.game_over = False
        self.last_move = None
        self.next_moves = list()
        self.current_player = "B"
        self.final_score = ""
        self.graph_data = None
        self._current_nid = None
        self.board_size = 19
        self.komi = 6.5

    @property
    def current_nid(self):
        return self._current_nid

    @property
    def current_node(self):
        return self.moves[self._current_nid]

    @property
    def next_ai_move(self):
        if analysis := self.current_node.data.get("analysis"):
            return analysis["next_ai_move"]

    @property
    def top_ai_moves(self):
        if analysis := self.current_node.data.get("analysis"):
            return analysis["moves"]

    @property
    def estimated_score(self):
        if analysis := self.current_node.data.get("analysis"):
            return analysis["estimated_score"]

    @property
    def ownership(self):
        if analysis := self.current_node.data.get("analysis"):
            return analysis["ownership"]

    @current_nid.setter
    def current_nid(self, value):
        self._current_nid = value
        self.last_board_state = self._last_board_state()
        self.prisoners = self._prisoners()
        self.last_move = self._last_move()
        self.game_over = self._game_over()
        try:
            self.graph_data = self._get_graph_data()
        except KeyError:
            pass

    def start(self):
        return self.record_move(("W", ""))

    def record_move(self, move):
        if self.current_nid:
            for child in self.moves.children(self.current_nid):
                if next(iter(child.data["move"])) == move:
                    self.current_nid = child.identifier
                    return
        self.current_nid = self.moves.create_node(
            data={
                "move": {move},
                "analysis": {},
                "captured_stones": (
                    captured_stones := self._calculate_captured_stones(
                        move, self.last_board_state
                    )
                ),
                "prisoners": self._calculate_prisoners(captured_stones),
            },
            parent=self.current_nid,
        ).identifier
        return self._request_analysis()

    def _calculate_captured_stones(self, move, state):
        if move[1] in ("pass", ""):
            return set()
        return set().union(
            *[
                set(self._group(m, state))
                for m in self._enemy_neighbours(move, state)
                if self._group_captured(self._group(m, state), state)
            ]
        )

    def _group(self, move, state):
        stones = state.copy()
        for m in self._group_branch(move, stones):
            yield m

    def _group_branch(self, move, state):
        try:
            state.remove(move)
            yield move
            for neighbour in self._friendly_neighbours(move, state):
                for m in self._group_branch(neighbour, state):
                    yield m
        except KeyError:
            pass

    def _friendly_neighbours(self, move, state):
        return {
            (move[0], position)
            for position in self._possible_neighbours(move[1])
            if (move[0], position) in state
        }

    def _enemy_neighbours(self, move, state):
        return {
            (self._other_color(move[0]), position)
            for position in self._possible_neighbours(move[1])
            if (self._other_color(move[0]), position) in state
        }

    def _possible_neighbours(self, position):
        cols = "ABCDEFGHJKLMNOPQRST"[: self.board_size]
        return {
            cols[cols.find(position[0]) + c] + str(int(position[1:]) + r)
            for c, r in [(-1, 0), (1, 0), (0, -1), (0, 1)]
            if 0 <= cols.find(position[0]) + c < self.board_size
            and 0 < int(position[1:]) + r <= self.board_size
        }

    def _group_captured(self, group, state):
        liberties = set()
        for move in group:
            liberties = (
                liberties.union(self._possible_neighbours(move[1]))
                - {m[1] for m in self._friendly_neighbours(move, state)}
                - {m[1] for m in self._enemy_neighbours(move, state)}
            )
            if len(liberties) > 1:
                return False
        return True

    def _calculate_prisoners(self, captured_stones):
        return {
            "black_stones": self.prisoners.get("black_stones", 0)
            + sum((1 for m in captured_stones if m[0] == "B")),
            "white_stones": self.prisoners.get("white_stones", 0)
            + sum((1 for m in captured_stones if m[0] == "W")),
        }

    def _get_graph_data(self):
        return [
            [
                {
                    "move": next(iter(self.moves[nid].data["move"])),
                    "score": self.moves[nid].data["analysis"]["estimated_score"],
                    "variations": self._variations(nid),
                    "is_current_move": nid == self.current_nid,
                    "identifier": nid,
                }
                for nid in path[1:]
            ]
            for path in self.moves.paths_to_leaves()
        ]

    def _variations(self, nid):
        siblings = sorted(
            self.moves.siblings(nid) + [self.moves[nid]],
            key=lambda s: next(iter(s.data["move"]))[1],
        )
        pos_of_nid = [node.identifier for node in siblings].index(nid)
        return [
            sib.identifier for sib in siblings[pos_of_nid:] + siblings[:pos_of_nid]
        ][1:]

    def _last_board_state(self):
        state = set()
        for nid in self._nid_path_to_current_node():
            state |= self._remove_pass(self.moves[nid].data["move"])
            state -= self.moves[nid].data["captured_stones"]
        return state

    def _nid_path_to_current_node(self):
        return list(reversed(list(self.moves.rsearch(self.current_nid))))[1:]

    @staticmethod
    def _remove_pass(moves):
        return {tuple(move) for move in moves if move[1] != "pass"}

    def _prisoners(self):
        return self.moves[self.current_nid].data["prisoners"]

    def _game_over(self):
        return ["pass", "pass"] == [move[1] for move in self.all_moves_as_list()[-2:]]

    def all_moves_as_list(self):
        return [
            list(next(iter(self.moves[nid].data["move"])))
            for nid in self._nid_path_to_current_node()
        ]

    def _all_captured_stones(self):
        return set().union(
            *[
                self.moves[nid].data["captured_stones"]
                for nid in self._nid_path_to_current_node()
            ]
        )

    def _last_move(self):
        last_move = next(iter(self.moves[self.current_nid].data["move"]))
        self.current_player = self._other_color(last_move[0])
        self.next_moves = [
            next(iter(child.data["move"]))
            for child in self.moves.children(self.current_nid)
        ]
        return last_move

    @staticmethod
    def _other_color(color):
        return "W" if color == "B" else "B"

    def _request_analysis(self):
        return json.dumps(
            {
                "query_id": self.current_nid,
                "moves": self.all_moves_as_list(),
            }
        )

    def set_analysis(self, result):
        nid = result["query_id"]
        if error := result.get("error"):
            self.remove_last_x_moves(1, nid)
            raise RuntimeError(f"Engine error {error}")
        else:
            try:
                self.moves[nid].data["analysis"] = result
            except NodeIDAbsentError:
                pass
            try:
                self.graph_data = self._get_graph_data()
                if self.game_over:
                    self.final_score = f"B {self._final_score()}"
            except KeyError:
                pass

    def remove_last_x_moves(self, x, nid=None):
        self.undo_last_x_moves(x, nid)
        for child in self.moves.children(self.current_nid):
            self.moves.remove_node(child.identifier)

    def undo_last_x_moves(self, x, nid=None):
        self.current_nid = list(self.moves.rsearch(nid or self.current_nid))[x]

    def _final_score(self):
        # reports for black
        board_ownership = {
            (v[0], k)
            for k, v in self.moves[self.current_nid]
            .data["analysis"]["ownership"]
            .items()
            if v[1] > 0.9
        }
        without_living_groups = sum(
            [
                1 if m[0] == "B" else -1
                for m in (board_ownership - self.last_board_state)
            ]
        )
        death_groups = sum(
            [
                1 if m[0] == "B" else -1
                for m in (self.last_board_state - board_ownership)
            ]
        )
        prisoners = self.prisoners
        return (
            without_living_groups
            - death_groups
            - self.komi
            - (prisoners["black_stones"] - prisoners["white_stones"])
        )

    def undo_stones(self, stones):
        if x := self._go_back_x_moves(stones):
            self.undo_last_x_moves(x)
        else:
            raise RuntimeError(f"Can not undo {stones}")

    def _go_back_x_moves(self, stones):
        for x in range(len(stones), self.moves.depth(self.current_nid) + 1):
            moves = (
                self._remove_pass(self.all_moves_as_list()[-x:])
                - self._all_captured_stones()
            )
            if len(moves) == len(stones):
                if moves == stones:
                    return x
                else:
                    return
