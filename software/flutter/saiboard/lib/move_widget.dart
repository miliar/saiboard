import 'package:flutter/material.dart';
class Move extends StatelessWidget {
  const Move({
    super.key,
    required this.color,
    required this.position,
  });
  final String? color;
  final String? position;
  Color get textColor => color == 'B' ? Colors.white : Colors.black;

  String get imagePath =>
      "assets/images/${color == 'W' ? 'white' : 'black'}.png";

  @override
  Widget build(BuildContext context) {
    return Stack(
      alignment: Alignment.center,
      children: <Widget>[
        Container(
            alignment: Alignment.center,
            height: 50,
            width: 50,
            child: Image.asset(imagePath)),
        Container(
          alignment: Alignment.center,
          child: Text(
            position ?? "",
            style: TextStyle(color: textColor),
          ),
        ),
      ],
    );
  }
}