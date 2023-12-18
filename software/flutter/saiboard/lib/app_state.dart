import 'package:flutter/material.dart';

class SaiboardAppState extends ChangeNotifier {
  dynamic _currentNodeData;
  dynamic get currentNodeData => _currentNodeData;
  set currentNodeData(dynamic data) {
    _currentNodeData = data;
    notifyListeners();
  }

  dynamic _graphData;
  dynamic get graphData => _graphData;
  set graphData(dynamic data) {
    _graphData = data;
    notifyListeners();
  }

  List<bool> _displayMode = <bool>[false, false, false];
  List<bool> get displayMode => _displayMode;
  void updateDisplayMode(int index) {
    for (int i = 0; i < _displayMode.length; i++) {
      _displayMode[i] = i == index ? !_displayMode[i] : false;
    }
    notifyListeners();
  }

  String _dropdownValue = 'Human vs Human';
  String get dropdownValue => _dropdownValue;
  set dropdownValue(String data) {
    _dropdownValue = data;
    notifyListeners();
  }
}
