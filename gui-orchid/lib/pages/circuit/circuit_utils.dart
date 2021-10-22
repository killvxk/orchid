import 'dart:async';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:orchid/api/orchid_api.dart';
import 'package:orchid/api/orchid_eth/orchid_account.dart';
import 'package:orchid/api/orchid_log_api.dart';
import 'package:orchid/api/preferences/user_preferences.dart';
import 'add_hop_page.dart';
import 'model/circuit.dart';
import 'model/circuit_hop.dart';
import 'model/orchid_hop.dart';

typedef HopCompletion = void Function(UniqueHop);

class CircuitUtils {
  // Show the add hop flow and save the result if completed successfully.
  static void addHop(BuildContext context, {HopCompletion onComplete}) async {
    // Create a nested navigation context for the flow. Performing a pop() from
    // this outer context at any point will properly remove the entire flow
    // (possibly multiple screens) with one appropriate animation.
    Navigator addFlow = Navigator(
      onGenerateRoute: (RouteSettings settings) {
        var addFlowCompletion = (CircuitHop result) {
          Navigator.pop(context, result);
        };
        var editor = AddHopPage(onAddFlowComplete: addFlowCompletion);
        var route = MaterialPageRoute<CircuitHop>(
            builder: (context) => editor, settings: settings);
        return route;
      },
    );
    var route = MaterialPageRoute<CircuitHop>(
        builder: (context) => addFlow, fullscreenDialog: true);
    _pushNewHopEditorRoute(context, route, onComplete);
  }

  // Push the specified hop editor to create a new hop, await the result, and
  // save it to the circuit.
  // Note: The paradigm here of the hop editors returning newly created or edited hops
  // Note: on the navigation stack decouples them but makes them more dependent on
  // Note: this update and save logic. We should consider centralizing this logic and
  // Note: relying on observation with `OrchidAPI.circuitConfigurationChanged` here.
  // Note: e.g.
  // Note: void _AddHopExternal(CircuitHop hop) async {
  // Note:   var circuit = await UserPreferences().getCircuit() ?? Circuit([]);
  // Note:   circuit.hops.add(hop);
  // Note:   await UserPreferences().setCircuit(circuit);
  // Note:   OrchidAPI().updateConfiguration();
  // Note:   OrchidAPI().circuitConfigurationChanged.add(null);
  // Note: }
  static void _pushNewHopEditorRoute(BuildContext context,
      MaterialPageRoute route, HopCompletion onComplete) async {
    var hop = await Navigator.push(context, route);
    if (hop == null) {
      return; // user cancelled
    }
    var uniqueHop =
        UniqueHop(hop: hop, key: DateTime.now().millisecondsSinceEpoch);
    addHopToCircuit(uniqueHop.hop);
    if (onComplete != null) {
      onComplete(uniqueHop);
    }
  }

  static void addHopToCircuit(CircuitHop hop) async {
    var circuit = await UserPreferences().getCircuit();
    circuit.hops.add(hop);
    saveCircuit(circuit);
  }

  /// Save the circuit and update published config and configuration listeners
  static Future<void> saveCircuit(Circuit circuit) async {
    try {
      log("XXX: Saving circuit: ${circuit.hops.map((e) => e.toJson())}");
      await UserPreferences().circuit.set(circuit);
    } catch (err, stack) {
      log("Error saving circuit: $err, $stack");
    }
    await OrchidAPI().updateConfiguration();
    OrchidAPI().circuitConfigurationChanged.add(null);
  }

  /// If the circuit is empty create a default single hop circuit using the
  /// supplied account.
  /// Returns true if a circuit was created.
  static Future<bool> defaultCircuitIfNeededFrom(Account account) async {
    var circuit = await UserPreferences().circuit.get();
    if (circuit.hops.isEmpty && account != null) {
      log("circuit: creating default circuit from account: $account");
      await CircuitUtils.saveCircuit(
        Circuit([OrchidHop.fromAccount(account)]),
      );
      return true;
    }
    return false;
  }
}
