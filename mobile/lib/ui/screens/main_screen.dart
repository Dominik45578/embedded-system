import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:lockly_app/core/services/auth.dart';
import 'package:lockly_app/ui/screens/login_screen.dart'; // Upewnij się co do ścieżki

class MainScreen extends StatelessWidget {
  final AuthRepository _authRepository = AuthRepository();

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<User?>(
      stream: _authRepository.userStream,
      builder: (context, snapshot) {
        // Jeśli Firebase jeszcze sprawdza sesję przy starcie
        if (snapshot.connectionState == ConnectionState.waiting) {
          return const Scaffold(
            body: Center(child: CircularProgressIndicator()),
          );
        }

        // Jeśli mamy użytkownika w strumieniu -> wyświetlamy Dashboard
        if (snapshot.hasData) {
          return _buildDashboard(context);
        }

        // Jeśli brak danych (wylogowany) -> wyświetlamy ekran logowania
        return const LoginScreen();
      },
    );
  }

  /// Widok wyświetlany po poprawnym zalogowaniu
  Widget _buildDashboard(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.white,
      body: SafeArea(
        child: Center(
          child: Column(
            children: [
              // Twój ostylowany napis
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
                decoration: BoxDecoration(
                  color: Colors.black,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: const Text(
                  "Zalogowano",
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
              const SizedBox(height: 20),
              // Przycisk wylogowania
              TextButton(
                onPressed: () async {
                  await _authRepository.signOut();
                  // Nie musisz tu robić Navigator.push - StreamBuilder sam to wykryje!
                },
                child: const Text(
                  "Wyloguj się",
                  style: TextStyle(color: Colors.red, fontWeight: FontWeight.w600),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}