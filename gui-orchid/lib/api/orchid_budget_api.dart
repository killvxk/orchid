import 'package:orchid/api/orchid_eth/token_type.dart';
import 'package:orchid/util/units.dart';
import 'orchid_crypto.dart';

class LotteryPot {
  final Token deposit;
  final Token balance;
  final BigInt unlock;
  final Token warned;

  DateTime get unlockTime {
    return DateTime.fromMillisecondsSinceEpoch(unlock.toInt() * 1000);
  }

  bool get isUnlocked {
    return unlock == BigInt.zero || unlockTime.isBefore(DateTime.now());
  }

  LotteryPot({
    this.deposit,
    this.balance,
    this.unlock,
    this.warned,
  });

  Token get maxTicketFaceValue {
    return maxTicketFaceValueFor(balance, deposit);
  }

  @override
  String toString() {
    return 'LotteryPot{deposit: $deposit, balance: $balance, unlock: $unlock, warned: $warned}';
  }

  static Token maxTicketFaceValueFor(Token balance, Token deposit) {
    return Token.min(balance, deposit / 2.0);
  }
}

/// Lottery pot balance and deposit amounts.
// Note: This supports migration of OXT-specific code. If we simply generalize
// Note: the value types to Token Dart would not catch assignment type errors
// Note: until runtime due to its automatic downcasting.
class OXTLotteryPot implements LotteryPot {
  final OXT deposit;
  final OXT balance;
  final BigInt unlock;
  final EthereumAddress verifier;

  OXTLotteryPot({
    this.deposit,
    this.balance,
    this.unlock,
    this.verifier,
  });

  OXT get maxTicketFaceValue {
    return maxTicketFaceValueFor(balance, deposit);
  }

  static OXT maxTicketFaceValueFor(OXT balance, OXT deposit) {
    return Token.min(balance, deposit / 2.0);
  }

  @override
  String toString() {
    return 'OXTLotteryPot{deposit: $deposit, balance: $balance}';
  }

  @override
  DateTime get unlockTime => throw UnimplementedError();

  @override
  Token get warned => throw UnimplementedError();

  @override
  bool get isUnlocked => throw UnimplementedError();
}

