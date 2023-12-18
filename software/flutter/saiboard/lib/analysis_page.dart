import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';
import 'package:charts_flutter/flutter.dart' as charts;
import 'package:web_socket_client/web_socket_client.dart';
import 'move_widget.dart';

class AnalysisPage extends StatelessWidget {
  const AnalysisPage({
    Key? key,
    required this.socket,
  }) : super(key: key);

  final WebSocket socket;

  List<List<Map<String, Object>>>? parseGraphData(dynamic graphData) {
    if (graphData == null) {
      return null;
    }
    if (graphData is List) {
      return graphData.map((innerList) {
        if (innerList is List) {
          return innerList.map((mapData) {
            if (mapData is Map) {
              return mapData.cast<String, Object>();
            }
            throw FormatException('Expected a map');
          }).toList();
        }
        throw FormatException('Expected a list');
      }).toList();
    }
    throw FormatException('Expected a list');
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.start,
        children: [
          Consumer<SaiboardAppState>(
            builder: (context, appState, child) {
              final graphData = parseGraphData(appState.graphData);
              return MoveGraph(graphData: graphData, socket: socket);
            },
          ),
          Expanded(child: Container()),
          DisplayButtons(socket: socket),
          const SizedBox(height: 48),
        ],
      ),
    );
  }
}

class MoveGraph extends StatefulWidget {
  const MoveGraph({
    super.key,
    required this.graphData,
    required this.socket,
  });

  final List<List<Map<String, Object>>>? graphData;
  final WebSocket socket;

  @override
  State<MoveGraph> createState() => _MoveGraphState();
}

class _MoveGraphState extends State<MoveGraph> {
  ScrollController _scrollController = ScrollController();
  List<Map<String, Object>> currentGraphData = [];

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    currentGraphData = _getCurrentGraphData();
  }

  int _getIndexOfCurrentMove() {
    return currentGraphData
        .indexWhere((move) => move['is_current_move'] == true);
  }

  List<Map<String, Object>> _getCurrentGraphData() {
    return widget.graphData?.firstWhere(
          (graph) => graph.any((move) => move['is_current_move'] == true),
          orElse: () => [],
        ) ??
        [];
  }

  List<Map<String, Object>> _generateScoreData(
      List<Map<String, Object>> currentGraphData) {
    final scoreData = [
      {'moveNumber': 0, 'score': 0.0},
      for (int i = 0; i < (currentGraphData.length); i++)
        {
          'moveNumber': i + 1,
          'score': double.parse(currentGraphData[i]['score'].toString()),
        }
    ];
    return scoreData;
  }

  @override
  Widget build(BuildContext context) {
    currentGraphData = _getCurrentGraphData();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _scrollController.jumpTo(_getIndexOfCurrentMove() * 54.0);
    });

    final scoreData = _generateScoreData(currentGraphData);
    return Column(
      children: [
        ScoreLineChart(
            scoreData: scoreData, scrollController: _scrollController),
        SizedBox(height: 30),
        MoveListView(
          scrollController: _scrollController,
          currentGraphData: currentGraphData,
          socket: widget.socket,
          graphData: widget.graphData,
        ),
      ],
    );
  }
}

class ScoreLineChart extends StatelessWidget {
  final List<Map<String, Object>> scoreData;
  final ScrollController scrollController;

  ScoreLineChart({required this.scoreData, required this.scrollController});

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.primary.withAlpha(50),
      ),
      child: SizedBox(
        height: 250,
        child: charts.LineChart(
            [
              charts.Series<Map<String, Object>, int>(
                id: 'Score',
                colorFn: (_, __) => charts.MaterialPalette.black,
                domainFn: (Map<String, Object> score, _) =>
                    score['moveNumber'] as int,
                measureFn: (Map<String, Object> score, _) =>
                    score['score'] as double,
                data: scoreData,
              ),
            ],
            animate: false,
            defaultRenderer: charts.LineRendererConfig(
              includeArea: true,
              stacked: true,
            ),
            domainAxis: charts.NumericAxisSpec(
              tickProviderSpec: charts.BasicNumericTickProviderSpec(
                desiredTickCount: 10,
              ),
            ),
            primaryMeasureAxis: charts.NumericAxisSpec(
              tickProviderSpec: charts.BasicNumericTickProviderSpec(
                desiredTickCount: 5,
              ),
            ),
            selectionModels: [
              charts.SelectionModelConfig(
                type: charts.SelectionModelType.info,
                changedListener: (model) {
                  if (model.hasDatumSelection) {
                    final selectedDatum = model.selectedDatum.first;
                    final selectedMoveNumber =
                        selectedDatum.datum['moveNumber'] as int;
                    final selectedMoveIndex = selectedMoveNumber - 1;
                    scrollController.animateTo(
                      selectedMoveIndex *
                          54.0, // Adjust the item height as needed
                      duration: Duration(milliseconds: 500),
                      curve: Curves.easeInOut,
                    );
                  }
                },
              ),
            ],
            layoutConfig: charts.LayoutConfig(
                leftMarginSpec: charts.MarginSpec.fixedPixel(30),
                topMarginSpec: charts.MarginSpec.fixedPixel(30),
                rightMarginSpec: charts.MarginSpec.fixedPixel(60),
                bottomMarginSpec: charts.MarginSpec.fixedPixel(30)),
            behaviors: [
              charts.RangeAnnotation([
                charts.RangeAnnotationSegment(
                  -10,
                  10,
                  charts.RangeAnnotationAxisType.measure,
                  startLabel: ' White',
                  endLabel: ' Black',
                  labelAnchor: charts.AnnotationLabelAnchor.end,
                  color: charts.MaterialPalette.transparent,
                ),
              ], defaultLabelPosition: charts.AnnotationLabelPosition.margin),
            ]),
      ),
    );
  }
}

class MoveListView extends StatelessWidget {
  final ScrollController scrollController;
  final List<dynamic> currentGraphData;
  final WebSocket socket;
  final List<List<Map<String, Object>>>? graphData;

  MoveListView({
    required this.scrollController,
    required this.currentGraphData,
    required this.socket,
    required this.graphData,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 54,
      child: ListView.builder(
        controller: scrollController,
        scrollDirection: Axis.vertical,
        itemCount: 1,
        itemBuilder: (context, _) {
          return Column(
            children: [
              for (dynamic item in currentGraphData.asMap().entries)
                GestureDetector(
                  onTap: () {
                    socket
                        .send('{"current_nid": "${item.value["identifier"]}"}');
                  },
                  child: Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Text(
                        "${item.key + 1}.",
                        style: TextStyle(
                          color: item.value["is_current_move"] == true
                              ? Theme.of(context).primaryColor
                              : Colors.black,
                        ),
                      ),
                      SizedBox(width: 10),
                      Container(
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            border: Border.all(
                              color: item.value["is_current_move"] == true
                                  ? Theme.of(context).primaryColor
                                  : Colors.transparent,
                              width: 2,
                            ),
                          ),
                          child: Move(
                            color: item.value["move"][0],
                            position: item.value["move"][1],
                          )),
                      SizedBox(width: 10),
                      Text(
                        double.parse(item.value["score"]).toStringAsFixed(2),
                        style: TextStyle(
                          color: item.value["is_current_move"] == true
                              ? Theme.of(context).primaryColor
                              : Colors.black,
                        ),
                      ),
                      SizedBox(width: 10),
                      if (item.value["variations"].length > 0)
                        VariationDropdown(
                          socket: socket,
                          variations: item.value["variations"],
                          graphData: graphData,
                        ),
                    ],
                  ),
                )
            ],
          );
        },
      ),
    );
  }
}

class VariationDropdown extends StatelessWidget {
  final WebSocket socket;
  final dynamic variations;
  final List<List<Map<String, Object>>>? graphData;

  VariationDropdown(
      {required this.socket,
      required this.variations,
      required this.graphData});
  Map<String, dynamic> _getMoveDataFromIdentifier(String identifier) {
    return graphData?.expand((x) => x).firstWhere(
            (move) => move['identifier'] == identifier,
            orElse: () => {}) ??
        {};
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: DropdownButton<String>(
        value: variations[0],
        onChanged: (String? newValue) {
          if (newValue != null) {
            socket.send('{"current_nid": "$newValue"}');
          }
        },
        underline: Container(),
        items: variations.map<DropdownMenuItem<String>>((dynamic value) {
          return DropdownMenuItem<String>(
              value: value,
              child: Padding(
                padding: const EdgeInsets.all(5.0),
                child: Move(
                  color: _getMoveDataFromIdentifier(value)["move"][0],
                  position: _getMoveDataFromIdentifier(value)["move"][1],
                ),
              ));
        }).toList(),
      ),
    );
  }
}

class DisplayButtons extends StatefulWidget {
  const DisplayButtons({
    Key? key,
    required this.socket,
  }) : super(key: key);

  final WebSocket socket;

  @override
  State<DisplayButtons> createState() => _DisplayButtonsState();
}

class _DisplayButtonsState extends State<DisplayButtons> {
  void toggleDisplayMode(int index) {
    String displayMode =
        Provider.of<SaiboardAppState>(context, listen: false).displayMode[index]
            ? ""
            : ['top_ai_moves', 'next_moves', 'ownership'][index];
    widget.socket.send('{"display_mode":"$displayMode"}');
    Provider.of<SaiboardAppState>(context, listen: false)
        .updateDisplayMode(index);
  }

  @override
  Widget build(BuildContext context) {
    List<bool> selectedDisplayMode =
        Provider.of<SaiboardAppState>(context).displayMode;

    return Align(
      alignment: Alignment.bottomCenter,
      child: ToggleButtons(
        isSelected: selectedDisplayMode,
        onPressed: (int index) => toggleDisplayMode(index),
        borderRadius: const BorderRadius.all(Radius.circular(8)),
        constraints: const BoxConstraints(
          minHeight: 40.0,
          minWidth: 90.0,
        ),
        children: const <Widget>[
          Text('Top moves'),
          Text('Next moves'),
          Text('Ownership'),
        ],
      ),
    );
  }
}
