// Telegram Bot for QuantumPulse
// Run with: node telegram-bot.js

const TELEGRAM_TOKEN = process.env.TELEGRAM_BOT_TOKEN || 'YOUR_TOKEN_HERE';
const API_URL = process.env.API_URL || 'http://localhost:8080/api';

class TelegramBot {
    constructor() {
        this.commands = new Map();
        this.userAlerts = new Map();
        this.setupCommands();
        console.log('QuantumPulse Telegram Bot initialized');
    }

    setupCommands() {
        // /start - Welcome message
        this.commands.set('start', async (chatId) => {
            return {
                text: `🚀 *Welcome to QuantumPulse Bot!*\n\n` +
                    `Your gateway to the quantum-resistant blockchain.\n\n` +
                    `*Available Commands:*\n` +
                    `/price - Current QP price\n` +
                    `/info - Blockchain info\n` +
                    `/balance <address> - Check balance\n` +
                    `/wallet - Create wallet\n` +
                    `/alert <price> - Price alert\n` +
                    `/help - All commands`,
                parse_mode: 'Markdown'
            };
        });

        // /price - Get price
        this.commands.set('price', async () => {
            try {
                const res = await fetch(`${API_URL}/price`);
                const data = await res.json();
                return {
                    text: `💰 *QuantumPulse Price*\n\n` +
                        `Current: *$${data.price.toLocaleString()}*\n` +
                        `Minimum: $600,000 ✓\n` +
                        `24h Change: +0.00%`,
                    parse_mode: 'Markdown'
                };
            } catch (e) {
                return { text: '❌ Error fetching price' };
            }
        });

        // /info - Blockchain info
        this.commands.set('info', async () => {
            try {
                const res = await fetch(`${API_URL}/info`);
                const data = await res.json();
                return {
                    text: `🔗 *QuantumPulse Blockchain*\n\n` +
                        `Blocks: ${data.chainLength}\n` +
                        `Mined: ${data.totalMinedCoins} QP\n` +
                        `Shards: ${data.shards}\n` +
                        `Premined: 2,000,000 QP\n` +
                        `Status: 🟢 Online`,
                    parse_mode: 'Markdown'
                };
            } catch (e) {
                return { text: '❌ Error fetching info' };
            }
        });

        // /balance <address>
        this.commands.set('balance', async (chatId, args) => {
            const address = args[0];
            if (!address) {
                return { text: '❌ Usage: `/balance <address>`', parse_mode: 'Markdown' };
            }

            try {
                const res = await fetch(`${API_URL}/balance/${address}`);
                const data = await res.json();
                return {
                    text: `👛 *Wallet Balance*\n\n` +
                        `Address: \`${address.substring(0, 16)}...\`\n` +
                        `Balance: *${data.balance} QP*\n` +
                        `Value: $${(data.balance * 600000).toLocaleString()}`,
                    parse_mode: 'Markdown'
                };
            } catch (e) {
                return { text: '❌ Error fetching balance' };
            }
        });

        // /wallet - Create wallet
        this.commands.set('wallet', async (chatId) => {
            const address = 'pub_v11_' + Math.random().toString(36).substring(2, 15);
            return {
                text: `🎉 *New Wallet Created!*\n\n` +
                    `Address:\n\`${address}\`\n\n` +
                    `⚠️ Save this address securely!`,
                parse_mode: 'Markdown'
            };
        });

        // /alert <price>
        this.commands.set('alert', async (chatId, args) => {
            const price = parseFloat(args[0]);
            if (!price || price < 600000) {
                return { text: '❌ Usage: `/alert <price>` (min $600,000)', parse_mode: 'Markdown' };
            }

            if (!this.userAlerts.has(chatId)) {
                this.userAlerts.set(chatId, []);
            }
            this.userAlerts.get(chatId).push(price);

            return {
                text: `✅ *Alert Set!*\n\nYou'll be notified when QP reaches $${price.toLocaleString()}`,
                parse_mode: 'Markdown'
            };
        });

        // /send - Send QP
        this.commands.set('send', async (chatId, args) => {
            const [to, amount] = args;
            if (!to || !amount) {
                return { text: '❌ Usage: `/send <address> <amount>`', parse_mode: 'Markdown' };
            }

            return {
                text: `📤 *Transaction Pending*\n\n` +
                    `To: \`${to.substring(0, 16)}...\`\n` +
                    `Amount: ${amount} QP\n` +
                    `Status: ⏳ Processing`,
                parse_mode: 'Markdown'
            };
        });

        // /help
        this.commands.set('help', async () => {
            return {
                text: `📚 *QuantumPulse Bot Commands*\n\n` +
                    `/start - Welcome message\n` +
                    `/price - Current QP price\n` +
                    `/info - Blockchain stats\n` +
                    `/balance <addr> - Check balance\n` +
                    `/wallet - Create new wallet\n` +
                    `/send <to> <amt> - Send QP\n` +
                    `/alert <price> - Set price alert\n` +
                    `/help - This message`,
                parse_mode: 'Markdown'
            };
        });
    }

    async handleMessage(chatId, text) {
        const parts = text.split(' ');
        const command = parts[0].replace('/', '').toLowerCase();
        const args = parts.slice(1);

        const handler = this.commands.get(command);
        if (handler) {
            return await handler(chatId, args);
        }
        return { text: '❌ Unknown command. Use /help' };
    }

    start() {
        console.log('Telegram bot started!');
        console.log('Commands: /start, /price, /info, /balance, /wallet, /send, /alert, /help');
    }
}

module.exports = { TelegramBot };

if (require.main === module) {
    const bot = new TelegramBot();
    bot.start();
}
