import React, { useState, useEffect } from 'react';
import {
    StyleSheet,
    Text,
    View,
    TouchableOpacity,
    ScrollView,
    TextInput,
    Alert,
    SafeAreaView
} from 'react-native';
import { StatusBar } from 'expo-status-bar';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';
import * as Clipboard from 'expo-clipboard';

const Tab = createBottomTabNavigator();

// Colors
const colors = {
    bg: '#0a0a0f',
    card: '#1a1a25',
    primary: '#6366f1',
    secondary: '#8b5cf6',
    success: '#10b981',
    text: '#ffffff',
    textSecondary: '#a0a0b0',
    border: 'rgba(255,255,255,0.1)',
};

// Dashboard Screen
function DashboardScreen() {
    const [price] = useState(600000);
    const [balance] = useState(2000000);

    return (
        <ScrollView style={styles.container}>
            <View style={styles.header}>
                <Text style={styles.logo}>⚡ QuantumPulse</Text>
                <Text style={styles.subtitle}>v7.0</Text>
            </View>

            <View style={styles.priceCard}>
                <Text style={styles.priceLabel}>QP Price</Text>
                <Text style={styles.priceValue}>${price.toLocaleString()}</Text>
                <Text style={styles.priceChange}>Guaranteed Minimum ✓</Text>
            </View>

            <View style={styles.statsRow}>
                <View style={styles.statCard}>
                    <Text style={styles.statValue}>16</Text>
                    <Text style={styles.statLabel}>Blocks</Text>
                </View>
                <View style={styles.statCard}>
                    <Text style={styles.statValue}>2M</Text>
                    <Text style={styles.statLabel}>Premined</Text>
                </View>
                <View style={styles.statCard}>
                    <Text style={styles.statValue}>35%</Text>
                    <Text style={styles.statLabel}>APY</Text>
                </View>
            </View>

            <View style={styles.card}>
                <Text style={styles.cardTitle}>📊 Network Stats</Text>
                <View style={styles.statItem}>
                    <Text style={styles.statItemLabel}>Hashrate</Text>
                    <Text style={styles.statItemValue}>150 MH/s</Text>
                </View>
                <View style={styles.statItem}>
                    <Text style={styles.statItemLabel}>Difficulty</Text>
                    <Text style={styles.statItemValue}>4</Text>
                </View>
                <View style={styles.statItem}>
                    <Text style={styles.statItemLabel}>Active Peers</Text>
                    <Text style={styles.statItemValue}>25</Text>
                </View>
            </View>
        </ScrollView>
    );
}

// Wallet Screen
function WalletScreen() {
    const [address] = useState('Shankar-Lal-Khati');
    const [balance] = useState(2000000);
    const [recipient, setRecipient] = useState('');
    const [amount, setAmount] = useState('');

    const copyAddress = async () => {
        await Clipboard.setStringAsync(address);
        Alert.alert('Copied!', 'Address copied to clipboard');
    };

    const sendTransaction = () => {
        if (!recipient || !amount) {
            Alert.alert('Error', 'Please enter recipient and amount');
            return;
        }
        Alert.alert('Transaction Sent!', `Sending ${amount} QP to ${recipient}`);
        setRecipient('');
        setAmount('');
    };

    return (
        <ScrollView style={styles.container}>
            <View style={styles.balanceCard}>
                <Text style={styles.balanceLabel}>Your Balance</Text>
                <Text style={styles.balanceValue}>{balance.toLocaleString()} QP</Text>
                <Text style={styles.balanceUsd}>${(balance * 600000).toLocaleString()} USD</Text>
            </View>

            <View style={styles.card}>
                <Text style={styles.cardTitle}>📬 Your Address</Text>
                <TouchableOpacity style={styles.addressBox} onPress={copyAddress}>
                    <Text style={styles.addressText}>{address}</Text>
                    <Text style={styles.copyHint}>Tap to copy</Text>
                </TouchableOpacity>
            </View>

            <View style={styles.card}>
                <Text style={styles.cardTitle}>📤 Send QP</Text>
                <TextInput
                    style={styles.input}
                    placeholder="Recipient address"
                    placeholderTextColor={colors.textSecondary}
                    value={recipient}
                    onChangeText={setRecipient}
                />
                <TextInput
                    style={styles.input}
                    placeholder="Amount"
                    placeholderTextColor={colors.textSecondary}
                    keyboardType="numeric"
                    value={amount}
                    onChangeText={setAmount}
                />
                <TouchableOpacity style={styles.primaryButton} onPress={sendTransaction}>
                    <Text style={styles.buttonText}>Send Transaction</Text>
                </TouchableOpacity>
            </View>
        </ScrollView>
    );
}

// Mining Screen
function MiningScreen() {
    const [isMining, setIsMining] = useState(false);
    const [hashrate] = useState(0);
    const [blocksFound] = useState(0);

    return (
        <ScrollView style={styles.container}>
            <View style={styles.card}>
                <Text style={styles.cardTitle}>⛏️ Mining Status</Text>
                <View style={styles.miningStatus}>
                    <Text style={styles.miningStatusText}>
                        {isMining ? '🟢 Mining Active' : '🔴 Mining Stopped'}
                    </Text>
                </View>

                <View style={styles.statsRow}>
                    <View style={styles.statCard}>
                        <Text style={styles.statValue}>{hashrate}</Text>
                        <Text style={styles.statLabel}>H/s</Text>
                    </View>
                    <View style={styles.statCard}>
                        <Text style={styles.statValue}>{blocksFound}</Text>
                        <Text style={styles.statLabel}>Blocks</Text>
                    </View>
                </View>

                <TouchableOpacity
                    style={[styles.primaryButton, isMining && styles.stopButton]}
                    onPress={() => setIsMining(!isMining)}
                >
                    <Text style={styles.buttonText}>{isMining ? 'Stop Mining' : 'Start Mining'}</Text>
                </TouchableOpacity>
            </View>

            <View style={styles.card}>
                <Text style={styles.cardTitle}>💰 Mining Rewards</Text>
                <View style={styles.statItem}>
                    <Text style={styles.statItemLabel}>Block Reward</Text>
                    <Text style={styles.statItemValue}>50 QP</Text>
                </View>
                <View style={styles.statItem}>
                    <Text style={styles.statItemLabel}>Mining Limit</Text>
                    <Text style={styles.statItemValue}>3,000,000 QP</Text>
                </View>
            </View>
        </ScrollView>
    );
}

// Settings Screen
function SettingsScreen() {
    return (
        <ScrollView style={styles.container}>
            <View style={styles.card}>
                <Text style={styles.cardTitle}>⚙️ Settings</Text>

                <TouchableOpacity style={styles.settingItem}>
                    <Text style={styles.settingText}>🔐 Enable 2FA</Text>
                    <Ionicons name="chevron-forward" size={20} color={colors.textSecondary} />
                </TouchableOpacity>

                <TouchableOpacity style={styles.settingItem}>
                    <Text style={styles.settingText}>🔔 Notifications</Text>
                    <Ionicons name="chevron-forward" size={20} color={colors.textSecondary} />
                </TouchableOpacity>

                <TouchableOpacity style={styles.settingItem}>
                    <Text style={styles.settingText}>🌙 Dark Mode</Text>
                    <Ionicons name="checkmark" size={20} color={colors.success} />
                </TouchableOpacity>

                <TouchableOpacity style={styles.settingItem}>
                    <Text style={styles.settingText}>📱 Biometric Lock</Text>
                    <Ionicons name="chevron-forward" size={20} color={colors.textSecondary} />
                </TouchableOpacity>
            </View>

            <View style={styles.card}>
                <Text style={styles.cardTitle}>ℹ️ About</Text>
                <Text style={styles.aboutText}>QuantumPulse v7.0</Text>
                <Text style={styles.aboutText}>Quantum-Resistant Blockchain</Text>
                <Text style={styles.aboutText}>© 2024 QuantumPulse</Text>
            </View>
        </ScrollView>
    );
}

// Main App
export default function App() {
    return (
        <NavigationContainer>
            <StatusBar style="light" />
            <Tab.Navigator
                screenOptions={({ route }) => ({
                    tabBarIcon: ({ focused, color, size }) => {
                        let iconName;
                        if (route.name === 'Dashboard') iconName = focused ? 'home' : 'home-outline';
                        else if (route.name === 'Wallet') iconName = focused ? 'wallet' : 'wallet-outline';
                        else if (route.name === 'Mining') iconName = focused ? 'hammer' : 'hammer-outline';
                        else if (route.name === 'Settings') iconName = focused ? 'settings' : 'settings-outline';
                        return <Ionicons name={iconName} size={size} color={color} />;
                    },
                    tabBarActiveTintColor: colors.primary,
                    tabBarInactiveTintColor: colors.textSecondary,
                    tabBarStyle: { backgroundColor: colors.card, borderTopColor: colors.border },
                    headerStyle: { backgroundColor: colors.bg },
                    headerTintColor: colors.text,
                })}
            >
                <Tab.Screen name="Dashboard" component={DashboardScreen} />
                <Tab.Screen name="Wallet" component={WalletScreen} />
                <Tab.Screen name="Mining" component={MiningScreen} />
                <Tab.Screen name="Settings" component={SettingsScreen} />
            </Tab.Navigator>
        </NavigationContainer>
    );
}

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: colors.bg, padding: 16 },
    header: { alignItems: 'center', marginBottom: 20, marginTop: 10 },
    logo: { fontSize: 28, fontWeight: 'bold', color: colors.text },
    subtitle: { fontSize: 14, color: colors.textSecondary },
    priceCard: {
        backgroundColor: colors.primary,
        borderRadius: 16,
        padding: 24,
        alignItems: 'center',
        marginBottom: 16
    },
    priceLabel: { fontSize: 14, color: 'rgba(255,255,255,0.8)' },
    priceValue: { fontSize: 36, fontWeight: 'bold', color: colors.text, marginVertical: 8 },
    priceChange: { fontSize: 14, color: colors.success },
    statsRow: { flexDirection: 'row', justifyContent: 'space-between', marginBottom: 16 },
    statCard: {
        flex: 1,
        backgroundColor: colors.card,
        borderRadius: 12,
        padding: 16,
        alignItems: 'center',
        marginHorizontal: 4
    },
    statValue: { fontSize: 24, fontWeight: 'bold', color: colors.text },
    statLabel: { fontSize: 12, color: colors.textSecondary, marginTop: 4 },
    card: {
        backgroundColor: colors.card,
        borderRadius: 16,
        padding: 20,
        marginBottom: 16,
        borderWidth: 1,
        borderColor: colors.border
    },
    cardTitle: { fontSize: 18, fontWeight: '600', color: colors.text, marginBottom: 16 },
    statItem: {
        flexDirection: 'row',
        justifyContent: 'space-between',
        paddingVertical: 12,
        borderBottomWidth: 1,
        borderBottomColor: colors.border
    },
    statItemLabel: { fontSize: 14, color: colors.textSecondary },
    statItemValue: { fontSize: 14, fontWeight: '600', color: colors.text },
    balanceCard: {
        backgroundColor: colors.card,
        borderRadius: 16,
        padding: 24,
        alignItems: 'center',
        marginBottom: 16,
        borderWidth: 1,
        borderColor: colors.primary
    },
    balanceLabel: { fontSize: 14, color: colors.textSecondary },
    balanceValue: { fontSize: 32, fontWeight: 'bold', color: colors.text, marginVertical: 8 },
    balanceUsd: { fontSize: 16, color: colors.success },
    addressBox: {
        backgroundColor: colors.bg,
        borderRadius: 12,
        padding: 16,
        alignItems: 'center'
    },
    addressText: { fontSize: 14, color: colors.primary, fontFamily: 'monospace' },
    copyHint: { fontSize: 12, color: colors.textSecondary, marginTop: 8 },
    input: {
        backgroundColor: colors.bg,
        borderRadius: 12,
        padding: 16,
        color: colors.text,
        marginBottom: 12,
        borderWidth: 1,
        borderColor: colors.border
    },
    primaryButton: {
        backgroundColor: colors.primary,
        borderRadius: 12,
        padding: 16,
        alignItems: 'center'
    },
    stopButton: { backgroundColor: '#ef4444' },
    buttonText: { color: colors.text, fontSize: 16, fontWeight: '600' },
    miningStatus: { alignItems: 'center', marginBottom: 16 },
    miningStatusText: { fontSize: 18, color: colors.text },
    settingItem: {
        flexDirection: 'row',
        justifyContent: 'space-between',
        alignItems: 'center',
        paddingVertical: 16,
        borderBottomWidth: 1,
        borderBottomColor: colors.border
    },
    settingText: { fontSize: 16, color: colors.text },
    aboutText: { fontSize: 14, color: colors.textSecondary, marginBottom: 8 },
});
