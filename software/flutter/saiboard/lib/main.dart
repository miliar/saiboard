import 'package:flutter/material.dart' hide ConnectionState;
import 'package:web_socket_client/web_socket_client.dart';
import 'package:provider/provider.dart';
import 'dart:convert';
import 'play_page.dart';
import 'analysis_page.dart';
import 'app_state.dart';

void main() {
  runApp(SaiboardApp());
}

class SaiboardApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (context) => SaiboardAppState(),
      child: MaterialApp(
        title: 'Saiboard',
        theme: ThemeData(
          useMaterial3: true,
          focusColor: Color.fromARGB(0, 0, 0, 0),
          colorScheme: ColorScheme.fromSeed(
              seedColor: Color.fromARGB(255, 172, 46, 211)),
          fontFamily: 'Roboto',
        ),
        home: MainNavigation(),
      ),
    );
  }
}

class MainNavigation extends StatefulWidget {
  @override
  State<MainNavigation> createState() => _MainNavigationState();
}

class _MainNavigationState extends State<MainNavigation> {
  final PageController controller = PageController();
  WebSocket socket = WebSocket(
      Uri.parse(
          'ws://192.168.4.5:7654'), // https://stackoverflow.com/questions/4779963/how-can-i-access-my-localhost-from-my-android-device
      timeout: Duration(seconds: 100),
      backoff: ConstantBackoff(Duration(seconds: 1)));
  int currentIndex = 0;
  List<Widget> tabPages = [];
  SocketHandler? socketHandler;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: PageView(
        controller: controller,
        children: tabPages,
        onPageChanged: (index) {
          setState(() {
            currentIndex = index;
          });
        },
      ),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: currentIndex,
        onTap: (index) {
          controller.jumpToPage(index);
          setState(() {
            currentIndex = index;
          });
        },
        items: const <BottomNavigationBarItem>[
          BottomNavigationBarItem(
            icon: Icon(Icons.play_arrow),
            label: 'Play',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.analytics),
            label: 'Analysis',
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    socket.close();
    super.dispose();
  }

  @override
  void initState() {
    super.initState();
    tabPages = [
      PlayPage(socket: socket),
      AnalysisPage(socket: socket),
    ];
    socketHandler = SocketHandler(socket, context);
  }
}

class SocketHandler {
  final WebSocket socket;
  final BuildContext context;

  SocketHandler(this.socket, this.context) {
    socket.connection.listen(handleConnectionStateChange);
    socket.messages.listen(handleSocketMessage);
  }
  void handleConnectionStateChange(ConnectionState state) {
    if (state is Connected || state is Reconnected) {
      handleConnectionEstablished();
    } else {
      handleConnectionLost();
    }
  }

  void handleConnectionEstablished() {
    socket.send('{"refresh_data": true}');
    popIfNotCurrent();
    ScaffoldMessenger.of(context)
        .showSnackBar(SnackBar(content: Text("Connected")));
  }

  void popIfNotCurrent() {
    if (ModalRoute.of(context)?.isCurrent != true) {
      Navigator.of(context).pop();
    }
  }

  void handleConnectionLost() {
    popIfNotCurrent();
    showErrorDialog("Connection lost");
  }

  Future<void> showErrorDialog(String message) async {
    return showDialog<void>(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          content: SingleChildScrollView(
            child: ListBody(
              children: <Widget>[
                Text(message),
              ],
            ),
          ),
          actions: <Widget>[
            TextButton(
              child: Text('Close'),
              onPressed: () {
                Navigator.of(context).pop();
              },
            ),
          ],
        );
      },
    );
  }

  void handleSocketMessage(dynamic data) {
    Map<String, dynamic> parsedData = json.decode(data);
    if (parsedData case {'graph': dynamic data}) {
      Provider.of<SaiboardAppState>(context, listen: false).graphData = data;
      return;
    }
    if (parsedData case {'current_node': dynamic data}) {
      Provider.of<SaiboardAppState>(context, listen: false).currentNodeData =
          data;
      return;
    }
    if (parsedData case {'button_states': dynamic data}) {
      handleButtonStates(data);
      return;
    }
    if (parsedData case {'message': String data}) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(data)));
      return;
    }
    if (parsedData case {'error': String data}) {
      handleError(data);
      return;
    }
    print("Unexpected json data $data");
  }

  void handleButtonStates(Map<String, dynamic> data) {
    int index =
        (data["display_mode"] as List<dynamic>).indexWhere((e) => e as bool);
    Provider.of<SaiboardAppState>(context, listen: false)
        .updateDisplayMode(index);
    Provider.of<SaiboardAppState>(context, listen: false).dropdownValue =
        "${data['players']['B']} vs ${data['players']['W']}";
  }

  void handleError(String data) {
    popIfNotCurrent();
    if (data == "resolved") {
      return;
    }
    showErrorDialog(data);
  }
}
