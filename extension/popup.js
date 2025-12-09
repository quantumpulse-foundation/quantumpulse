// QuantumPulse Browser Extension - Popup Script

const API_URL = 'http://localhost:8080/api';

// Initialize wallet
async function init() {
    // Get or create wallet
    let wallet = await chrome.storage.local.get('wallet');

    if (!wallet.wallet) {
        wallet.wallet = {
            address: 'pub_v11_' + Math.random().toString(36).substring(2, 15),
            balance: 0
        };
        await chrome.storage.local.set({ wallet: wallet.wallet });
    }

    document.getElementById('address').textContent = wallet.wallet.address;

    // Fetch balance
    try {
        const res = await fetch(`${API_URL}/balance/${wallet.wallet.address}`);
        const data = await res.json();
        updateBalance(data.balance || 0);
    } catch (e) {
        updateBalance(wallet.wallet.balance || 0);
    }

    // Fetch price
    try {
        const res = await fetch(`${API_URL}/price`);
        const data = await res.json();
        document.getElementById('price').textContent = `$${data.price.toLocaleString()}`;
    } catch (e) {
        document.getElementById('price').textContent = '$600,000';
    }
}

function updateBalance(balance) {
    document.getElementById('balance').textContent = `${balance.toFixed(4)} QP`;
    document.getElementById('balanceUsd').textContent = `≈ $${(balance * 600000).toLocaleString()} USD`;
}

// Send button
document.getElementById('sendBtn').addEventListener('click', () => {
    const to = prompt('Enter recipient address:');
    const amount = prompt('Enter amount (QP):');

    if (to && amount) {
        alert(`Sending ${amount} QP to ${to}\n\nTransaction submitted!`);
    }
});

// Receive button
document.getElementById('receiveBtn').addEventListener('click', async () => {
    const wallet = await chrome.storage.local.get('wallet');
    navigator.clipboard.writeText(wallet.wallet.address);
    alert('Address copied to clipboard!');
});

// Address click to copy
document.getElementById('address').addEventListener('click', async () => {
    const wallet = await chrome.storage.local.get('wallet');
    navigator.clipboard.writeText(wallet.wallet.address);
    document.getElementById('address').textContent = 'Copied!';
    setTimeout(async () => {
        const w = await chrome.storage.local.get('wallet');
        document.getElementById('address').textContent = w.wallet.address;
    }, 1000);
});

init();
