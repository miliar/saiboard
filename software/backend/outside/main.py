import asyncio
import websockets
import aioredis

connected_websockets = set()


async def main(redis_url, ws_host, ws_port, channel_to_ws, channel_from_ws):
    redis = await aioredis.from_url(redis_url)
    ws_to_redis = await websockets.serve(
        lambda websocket: _from_ws_to_redis(websocket, redis, channel_from_ws),
        host=ws_host,
        port=ws_port,
    )
    redis_to_ws = await _from_redis_to_ws(redis, channel_to_ws)
    await asyncio.gather(ws_to_redis, redis_to_ws)


async def _from_ws_to_redis(websocket, redis, channel):
    connected_websockets.add(websocket)
    try:
        async for message in websocket:
            print(f"Received: {message}")
            await redis.publish(channel, message)
    except websockets.exceptions.ConnectionClosedOK:
        print("Connection closed")
    finally:
        connected_websockets.remove(websocket)


async def _from_redis_to_ws(redis, channel):
    pubsub = redis.pubsub()
    await pubsub.subscribe(channel)
    async for message in pubsub.listen():
        if message["type"] != "message":
            continue
        data = message["data"].decode("utf-8")
        for websocket in connected_websockets:
            try:
                await websocket.send(data)
                print(f"Send: {data}")
            except websockets.exceptions.ConnectionClosedOK:
                connected_websockets.remove(websocket)


if __name__ == "__main__":
    asyncio.get_event_loop().run_until_complete(
        main(
            redis_url="redis://redis:6379",
            ws_host="0.0.0.0",
            ws_port=7654,
            channel_to_ws="game",
            channel_from_ws="outside",
        )
    )
