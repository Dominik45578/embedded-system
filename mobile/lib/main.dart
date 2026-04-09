import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';
import 'ui/app.dart';
import 'firebase_options.dart'; // Zaimportuj wygenerowany plik


void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(); // To łączy się z Twoim JSON-em
  runApp(MyApp());
}