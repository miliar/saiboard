from board import Board


if __name__ == "__main__":
    board = Board(
        host="192.168.4.1",
        port=3333,
        redis_host="redis",
        redis_port=6379,
        queue_out="board_out",
        queue_in="board_in",
        board_size=19,
        threshold_white=40,
        threshold_black=-40,
        threshold_touch=3200,
        nr_of_boot_up_rounds=20,
        state_cache_len=4,
        socket_timeout=5.0,
        touch_correct_factor=20,
        data_end_marker=b"\x00",
    )
    board.run()
