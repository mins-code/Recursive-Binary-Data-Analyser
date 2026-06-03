const express = require('express');
const path = require('path');
const app = express();
const PORT = 5000;

const analyzeRoute = require('./routes/analyze');

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

app.use('/', analyzeRoute);

app.listen(PORT, () => {
    console.log(`RBDA dashboard running at http://localhost:${PORT}`);
});