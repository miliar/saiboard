import json
import subprocess
import time
from threading import Thread


class KataGo:
    def __init__(self, katago_path, config_path, model_path):
        katago = subprocess.Popen(
            [katago_path, "analysis", "-config", config_path, "-model", model_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.katago = katago

        def printforever():
            while katago.poll() is None:
                data = katago.stderr.readline()
                time.sleep(0)
                if data:
                    print("KataGo: ", data.decode(), end="")
            data = katago.stderr.read()
            if data:
                print("KataGo: ", data.decode(), end="")

        self.stderrthread = Thread(target=printforever)
        self.stderrthread.start()

    def close(self):
        self.katago.stdin.close()

    def query(
        self,
        moves,
        query_id,
        initial_stones=[],
        rules="japanese",
        komi=6.5,
        board_size=19,
        initial_player="B",
    ):
        query = {
            "initialPlayer": initial_player,
            "id": query_id,
            "moves": moves,
            "initialStones": initial_stones,
            "rules": rules,
            "komi": komi,
            "boardXSize": board_size,
            "boardYSize": board_size,
            "includeOwnership": True,
        }
        self.katago.stdin.write((json.dumps(query) + "\n").encode())
        self.katago.stdin.flush()
        line = ""
        while line == "":
            if self.katago.poll():
                time.sleep(1)
                raise Exception("Unexpected katago exit")
            line = self.katago.stdout.readline()
            line = line.decode().strip()
        return self._transform(json.loads(line), board_size)

    def _transform(self, report, board_size):
        try:
            return {
                "query_id": report["id"],
                "next_ai_move": [
                    report["rootInfo"]["currentPlayer"],
                    report["moveInfos"][0]["move"],
                ],
                "estimated_score": f"{report['rootInfo']['scoreLead']}",
                "moves": [
                    {
                        "move": move["move"],
                        "score_change": move["scoreLead"]
                        - report["rootInfo"]["scoreLead"],
                    }
                    for move in report["moveInfos"]
                ],
                "ownership": self._ownership(
                    report.get("ownership"),
                    "B",  # In config: reportAnalysisWinratesAs = BLACK
                    board_size,
                ),
            }
        except KeyError:
            return {"error": report["error"]}

    def _ownership(self, ownership, current_player, board_size):
        if not ownership:
            return
        other_player = "B" if current_player == "W" else "W"
        return dict(
            zip(
                [
                    col + row
                    for row in reversed([str(nr + 1) for nr in range(board_size)])
                    for col in "ABCDEFGHJKLMNOPQRST"[:board_size]
                ],
                [
                    (current_player, abs(v)) if v >= 0.0 else (other_player, abs(v))
                    for v in ownership
                ],
            )
        )
