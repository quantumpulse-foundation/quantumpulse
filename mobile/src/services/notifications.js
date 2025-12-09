import * as Notifications from 'expo-notifications';
import * as Device from 'expo-device';
import { Platform } from 'react-native';

// Configure notification handler
Notifications.setNotificationHandler({
    handleNotification: async () => ({
        shouldShowAlert: true,
        shouldPlaySound: true,
        shouldSetBadge: true,
    }),
});

// Notification types
export const NotificationType = {
    TRANSACTION_RECEIVED: 'transaction_received',
    TRANSACTION_SENT: 'transaction_sent',
    TRANSACTION_CONFIRMED: 'transaction_confirmed',
    PRICE_ALERT: 'price_alert',
    MINING_COMPLETE: 'mining_complete',
    SECURITY_ALERT: 'security_alert',
};

// Register for push notifications
export async function registerForPushNotifications() {
    let token;

    if (Device.isDevice) {
        const { status: existingStatus } = await Notifications.getPermissionsAsync();
        let finalStatus = existingStatus;

        if (existingStatus !== 'granted') {
            const { status } = await Notifications.requestPermissionsAsync();
            finalStatus = status;
        }

        if (finalStatus !== 'granted') {
            console.log('Push notification permission denied');
            return null;
        }

        token = (await Notifications.getExpoPushTokenAsync()).data;
        console.log('Push token:', token);
    } else {
        console.log('Push notifications require physical device');
    }

    if (Platform.OS === 'android') {
        Notifications.setNotificationChannelAsync('default', {
            name: 'QuantumPulse',
            importance: Notifications.AndroidImportance.MAX,
            vibrationPattern: [0, 250, 250, 250],
            lightColor: '#6366f1',
        });
    }

    return token;
}

// Schedule local notification
export async function scheduleNotification(title, body, type, data = {}) {
    await Notifications.scheduleNotificationAsync({
        content: {
            title,
            body,
            data: { type, ...data },
            sound: true,
            badge: 1,
        },
        trigger: null, // Immediate
    });
}

// Transaction received notification
export async function notifyTransactionReceived(amount, from) {
    await scheduleNotification(
        '💰 QP Received!',
        `You received ${amount} QP from ${from.substring(0, 12)}...`,
        NotificationType.TRANSACTION_RECEIVED,
        { amount, from }
    );
}

// Transaction sent notification
export async function notifyTransactionSent(amount, to, txId) {
    await scheduleNotification(
        '📤 Transaction Sent',
        `Sent ${amount} QP to ${to.substring(0, 12)}...`,
        NotificationType.TRANSACTION_SENT,
        { amount, to, txId }
    );
}

// Transaction confirmed notification
export async function notifyTransactionConfirmed(txId) {
    await scheduleNotification(
        '✅ Transaction Confirmed',
        `Transaction ${txId.substring(0, 16)}... has been confirmed`,
        NotificationType.TRANSACTION_CONFIRMED,
        { txId }
    );
}

// Price alert notification
export async function notifyPriceAlert(price, threshold, direction) {
    const arrow = direction === 'up' ? '📈' : '📉';
    await scheduleNotification(
        `${arrow} QP Price Alert`,
        `QP price ${direction === 'up' ? 'above' : 'below'} $${threshold.toLocaleString()} - Now: $${price.toLocaleString()}`,
        NotificationType.PRICE_ALERT,
        { price, threshold, direction }
    );
}

// Mining complete notification
export async function notifyMiningComplete(reward, blockHeight) {
    await scheduleNotification(
        '⛏️ Block Mined!',
        `Successfully mined block #${blockHeight}. Reward: ${reward} QP`,
        NotificationType.MINING_COMPLETE,
        { reward, blockHeight }
    );
}

// Security alert notification
export async function notifySecurityAlert(message) {
    await scheduleNotification(
        '🔐 Security Alert',
        message,
        NotificationType.SECURITY_ALERT,
        { urgent: true }
    );
}

// Add notification listener
export function addNotificationListener(callback) {
    return Notifications.addNotificationReceivedListener(callback);
}

// Add response listener (when user taps notification)
export function addResponseListener(callback) {
    return Notifications.addNotificationResponseReceivedListener(callback);
}

// Cancel all notifications
export async function cancelAllNotifications() {
    await Notifications.cancelAllScheduledNotificationsAsync();
}

// Get badge count
export async function getBadgeCount() {
    return await Notifications.getBadgeCountAsync();
}

// Set badge count
export async function setBadgeCount(count) {
    await Notifications.setBadgeCountAsync(count);
}

export default {
    registerForPushNotifications,
    scheduleNotification,
    notifyTransactionReceived,
    notifyTransactionSent,
    notifyTransactionConfirmed,
    notifyPriceAlert,
    notifyMiningComplete,
    notifySecurityAlert,
    addNotificationListener,
    addResponseListener,
    cancelAllNotifications,
    getBadgeCount,
    setBadgeCount,
};
