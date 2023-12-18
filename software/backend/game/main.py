from play import Play


if __name__ == "__main__":
    play = Play(redis_host="redis", redis_port=6379)
    play.start_game()
