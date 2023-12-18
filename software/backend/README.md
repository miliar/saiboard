# Overview
Backend runs 4 containers.
- [board](https://github.com/miliar/saiboard/tree/main/software/backend/board) interfaces with the physical board.
   <img width="777" alt="Screenshot 2023-12-17 at 13 18 52" src="https://github.com/miliar/saiboard/assets/35922697/0fd4d0d3-62f8-467c-bc28-b56a0d3b949d">
 

- [katago](https://github.com/miliar/saiboard/tree/main/software/backend/katago) is running the [katago analysis engine](https://github.com/lightvector/KataGo/blob/master/docs/Analysis_Engine.md) inside a docker container. There a 2 Dockerfiles for different host machine architectures
  <img width="1440" alt="Screenshot 2023-12-17 at 13 17 39" src="https://github.com/miliar/saiboard/assets/35922697/9ebc9b98-41db-4d0d-97dd-9dbb79a504cf">

- [game](https://github.com/miliar/saiboard/tree/main/software/backend/game) contains all game logic.
   <img width="996" alt="Screenshot 2023-12-17 at 13 18 05" src="https://github.com/miliar/saiboard/assets/35922697/3cb03965-841c-4a7c-9bf0-f7b75e93cf6d">

- [outside](https://github.com/miliar/saiboard/tree/main/software/backend/board) interfaces with the UI.

  
It is deployed to a raspberry pi, but could also run from a more powerful server.
### Katago benchmark on raspberry pi 4
<img width="800" alt="benchmark_rp4" src="https://github.com/miliar/saiboard/assets/35922697/f07dca0f-258a-4f3a-a143-fd0effcf0f41">
