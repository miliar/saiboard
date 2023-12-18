import redis
from katago import KataGo
import json


if __name__ == "__main__":
    redis_conn = redis.Redis(host="redis", port=6379)
    pubsub = redis_conn.pubsub()
    pubsub.subscribe("katago_in")
    katago = KataGo(
        katago_path="/workspace/katago/katago",
        model_path="/workspace/katago/kata1-b18c384nbt-s6582191360-d3422816034.bin.gz",
        config_path="analysis.cfg",
    )

    try:
        for message in pubsub.listen():
            if message["type"] != "message":
                continue
            request = json.loads(message["data"].decode())
            print(request)
            query = katago.query(**request)
            print(query)
            if not query.get("query_id"):
                query["query_id"] = request["query_id"]
            redis_conn.publish("katago_out", json.dumps(query))
    finally:
        katago.close()
