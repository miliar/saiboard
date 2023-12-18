import 'package:flutter/material.dart';
import 'package:web_socket_client/web_socket_client.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';
import 'move_widget.dart';

class PlayPage extends StatefulWidget {
  const PlayPage({
    super.key,
    required this.socket,
  });
  final WebSocket socket;

  @override
  State<PlayPage> createState() => _PlayPageState();
}

class _PlayPageState extends State<PlayPage> {
  final List<String> gameOptions = <String>[
    'Human vs Human',
    'Human vs AI',
    'AI vs Human',
    'AI vs AI'
  ];
  late String dropdownValue;

  @override
  Widget build(BuildContext context) {
    dropdownValue = Provider.of<SaiboardAppState>(context).dropdownValue;
    final currentNodeData =
        Provider.of<SaiboardAppState>(context).currentNodeData;
    return Center(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SizedBox(height: 24),
          GameOptions(
            dropdownValue: dropdownValue,
            gameOptions: gameOptions,
            onDropdownChanged: (String? newValue) {
              setState(() {
                Provider.of<SaiboardAppState>(context, listen: false)
                    .dropdownValue = newValue!;
              });
              widget.socket.send(
                  '{"new_game":{"player_b": "${newValue!.split(' ')[0]}", "player_w":"${newValue.split(' ')[2]}"}}');
              widget.socket.send('{"refresh_data": true}');
            },
            socket: widget.socket,
          ),
          const SizedBox(height: 24),
          MoveDisplay(currentNodeData: currentNodeData),
          const SizedBox(height: 48),
          PrisonersDisplay(currentNodeData: currentNodeData),
          const SizedBox(height: 96),
          PassButton(socket: widget.socket),
        ],
      ),
    );
  }
}

class GameOptions extends StatelessWidget {
  final String dropdownValue;
  final List<String> gameOptions;
  final Function(String?) onDropdownChanged;
  final WebSocket socket;

  const GameOptions({
    Key? key,
    required this.dropdownValue,
    required this.gameOptions,
    required this.onDropdownChanged,
    required this.socket,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        Container(
          alignment: Alignment.center,
          height: 20,
          width: 20,
          child: Image.asset("assets/images/black.png"),
        ),
        Center(
          child: DropdownButton<String>(
            value: dropdownValue,
            icon: const Icon(Icons.play_circle_outline_sharp),
            iconSize: 24,
            elevation: 16,
            iconEnabledColor: Theme.of(context).primaryColor,
            onChanged: onDropdownChanged,
            items: gameOptions.map<DropdownMenuItem<String>>((String value) {
              return DropdownMenuItem<String>(
                value: value,
                child: Text(value, style: const TextStyle(fontSize: 20)),
              );
            }).toList(),
          ),
        ),
        Container(
          alignment: Alignment.center,
          height: 20,
          width: 20,
          child: Image.asset("assets/images/white.png"),
        ),
      ],
    );
  }
}

class MoveDisplay extends StatelessWidget {
  final Map<String, dynamic>? currentNodeData;

  const MoveDisplay({
    Key? key,
    this.currentNodeData,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Container(
        alignment: Alignment.center,
        height: 70,
        width: 125,
        child: Stack(
          alignment: Alignment.center,
          children: [
            Move(
                position: currentNodeData?["move"][0][1],
                color: currentNodeData?["move"][0][0]),
            if (currentNodeData != null)
              Positioned(
                left: 75.0,
                child: Move(
                    position: "?",
                    color: currentNodeData?["move"][0][0] == "W" ? "B" : "W"),
              ),
          ],
        ),
      ),
    );
  }
}

class PrisonersDisplay extends StatelessWidget {
  final Map<String, dynamic>? currentNodeData;

  const PrisonersDisplay({
    Key? key,
    this.currentNodeData,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        Text("${currentNodeData?['prisoners']['white_stones'] ?? '0'}"),
        Text("Prisoners"),
        Text("${currentNodeData?['prisoners']['black_stones'] ?? '0'}"),
      ],
    );
  }
}

class PassButton extends StatelessWidget {
  final WebSocket socket;

  const PassButton({
    Key? key,
    required this.socket,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Center(
      child: ElevatedButton(
        onPressed: () {
          socket.send('{"pass": true}');
        },
        child: const Text('Pass'),
      ),
    );
  }
}
