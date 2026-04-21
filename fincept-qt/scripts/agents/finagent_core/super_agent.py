"""
SuperAgent - Triage routing for query distribution

Routes queries to appropriate sub-agents based on intent classification.
Supports two routing modes:
  - LLM-based routing (default): The LLM itself decides which agent/intent fits best.
    This is fully agentic — no hardcoded keyword tables.
  - Keyword fallback: Used when no LLM config is available (offline / no API key).
"""

import re
import json
from typing import Dict, Any, Optional, List, Callable
from dataclasses import dataclass, field
from enum import Enum
import logging

logger = logging.getLogger(__name__)

# System prompt given to the LLM router — lists all intents and asks for a JSON decision
LLM_ROUTER_SYSTEM_PROMPT = """你是一个金融查询路由器。你的唯一任务是将用户查询分类到以下意图之一，并返回一个 JSON 对象。

可用意图：
- trading: 订单执行、买入/卖出信号、入场/出场策略、市价单
- portfolio: 投资组合配置、再平衡、多元化、持仓管理
- analysis: 股票/权益研究、估值（DCF, P/E）、基本面或技术分析、目标价
- risk: VaR、波动率、最大回撤、夏普比率、对冲、压力测试、相关性
- news: 市场新闻、盈利公告、新闻稿、头条新闻
- geopolitics: 地缘政治风险、国际关系、制裁、贸易战、选举
- economics: GDP、通胀、利率、中央银行、就业、CPI/PPI
- research: 深度研究、行业研究、综合调查
- general: 不属于上述任何一类的其他问题

请仅以以下格式返回有效的 JSON 对象（不要使用 markdown，不要有任何解释）：
{"intent": "<intent>", "confidence": <0.0-1.0>, "reasoning": "<一段简短的中文分类理由>"}"""


class QueryIntent(Enum):
    """Classified query intents"""
    TRADING = "trading"
    PORTFOLIO = "portfolio"
    ANALYSIS = "analysis"
    RISK = "risk"
    NEWS = "news"
    GEOPOLITICS = "geopolitics"
    ECONOMICS = "economics"
    RESEARCH = "research"
    GENERAL = "general"


@dataclass
class RouteConfig:
    """Configuration for a route"""
    intent: QueryIntent
    agent_id: str
    keywords: List[str] = field(default_factory=list)
    patterns: List[str] = field(default_factory=list)
    priority: int = 0
    config_override: Dict[str, Any] = field(default_factory=dict)


@dataclass
class RoutingResult:
    """Result of routing decision"""
    intent: QueryIntent
    agent_id: str
    confidence: float
    config: Dict[str, Any]
    matched_keywords: List[str] = field(default_factory=list)
    matched_patterns: List[str] = field(default_factory=list)


class IntentClassifier:
    """
    Classifies query intent using keyword matching and patterns.
    Can be extended with ML-based classification.
    """

    # Default keyword mappings
    DEFAULT_KEYWORDS = {
        QueryIntent.TRADING: [
            "buy", "sell", "trade", "order", "position", "long", "short",
            "stop loss", "take profit", "limit order", "market order",
            "entry", "exit", "fill", "execute"
        ],
        QueryIntent.PORTFOLIO: [
            "portfolio", "holdings", "allocation", "rebalance", "diversify",
            "weight", "exposure", "balance", "assets", "positions"
        ],
        QueryIntent.ANALYSIS: [
            "analyze", "analysis", "valuation", "dcf", "fair value",
            "fundamental", "technical", "chart", "indicator", "pattern",
            "support", "resistance", "trend", "investment thesis",
            "stock analysis", "equity research", "price target", "outlook",
            "forecast", "recommendation", "rating", "overvalued", "undervalued"
        ],
        QueryIntent.RISK: [
            "risk", "var", "volatility", "drawdown", "sharpe", "beta",
            "correlation", "hedge", "exposure", "stress test", "scenario"
        ],
        QueryIntent.NEWS: [
            "news", "headline", "announcement", "earnings", "report",
            "press release", "update", "breaking"
        ],
        QueryIntent.GEOPOLITICS: [
            "geopolitical", "war", "conflict", "sanction", "trade war",
            "election", "policy", "government", "diplomatic", "military"
        ],
        QueryIntent.ECONOMICS: [
            "gdp", "inflation", "interest rate", "fed", "central bank",
            "employment", "jobs", "cpi", "ppi", "economic"
        ],
        QueryIntent.RESEARCH: [
            "research", "deep dive", "investigate", "study", "comprehensive",
            "detailed", "in-depth", "explore"
        ]
    }

    # Default patterns
    DEFAULT_PATTERNS = {
        QueryIntent.TRADING: [
            r'\b(buy|sell)\s+\d+',
            r'\b(long|short)\s+[A-Z]{1,5}\b',
            r'place\s+(market|limit)\s+order'
        ],
        QueryIntent.ANALYSIS: [
            r'analyze\s+[A-Z]{1,5}',
            r'what.*think.*about\s+[A-Z]{1,5}',
            r'[A-Z]{1,5}\s+(outlook|forecast|prediction)'
        ],
        QueryIntent.PORTFOLIO: [
            r'my\s+portfolio',
            r'rebalance.*portfolio',
            r'portfolio.*allocation'
        ]
    }

    def __init__(
        self,
        custom_keywords: Dict[QueryIntent, List[str]] = None,
        custom_patterns: Dict[QueryIntent, List[str]] = None
    ):
        self.keywords = {**self.DEFAULT_KEYWORDS}
        self.patterns = {**self.DEFAULT_PATTERNS}

        if custom_keywords:
            for intent, kws in custom_keywords.items():
                self.keywords.setdefault(intent, []).extend(kws)

        if custom_patterns:
            for intent, pts in custom_patterns.items():
                self.patterns.setdefault(intent, []).extend(pts)

        # Compile patterns
        self._compiled_patterns = {
            intent: [re.compile(p, re.IGNORECASE) for p in patterns]
            for intent, patterns in self.patterns.items()
        }

    def classify(self, query: str) -> List[tuple[QueryIntent, float, List[str], List[str]]]:
        """
        Classify query intent.

        Returns:
            List of (intent, confidence, matched_keywords, matched_patterns) sorted by confidence
        """
        query_lower = query.lower()
        results = []

        for intent in QueryIntent:
            matched_keywords = []
            matched_patterns = []

            # Check keywords
            if intent in self.keywords:
                for kw in self.keywords[intent]:
                    if kw.lower() in query_lower:
                        matched_keywords.append(kw)

            # Check patterns
            if intent in self._compiled_patterns:
                for pattern in self._compiled_patterns[intent]:
                    if pattern.search(query):
                        matched_patterns.append(pattern.pattern)

            # Calculate confidence
            if matched_keywords or matched_patterns:
                keyword_score = len(matched_keywords) * 0.3
                pattern_score = len(matched_patterns) * 0.5
                confidence = min(1.0, keyword_score + pattern_score)
                results.append((intent, confidence, matched_keywords, matched_patterns))

        # Sort by confidence descending
        results.sort(key=lambda x: x[1], reverse=True)

        # If no matches, return GENERAL
        if not results:
            results.append((QueryIntent.GENERAL, 0.1, [], []))

        return results


class LLMRouter:
    """
    True agentic router: asks an LLM to classify the query intent.

    Falls back to IntentClassifier (keyword-based) if:
    - No model config / API keys are available
    - LLM call fails or times out
    - LLM returns unparseable output
    """

    INTENT_VALUES = {i.value for i in QueryIntent}

    def __init__(
        self,
        model_config: Optional[Dict[str, Any]] = None,
        api_keys: Optional[Dict[str, str]] = None,
    ):
        self.model_config = model_config or {}
        self.api_keys = api_keys or {}
        self._fallback = IntentClassifier()

    def classify(self, query: str) -> tuple:
        """
        Classify query intent using LLM if available, else keyword fallback.

        Returns:
            (QueryIntent, confidence: float, reasoning: str)
        """
        provider = self.model_config.get("provider", "")
        model_id = self.model_config.get("model_id", "")

        if provider and model_id:
            try:
                return self._llm_classify(query, provider, model_id)
            except Exception as e:
                logger.warning(f"LLM routing failed, using keyword fallback: {e}")

        # Keyword fallback
        results = self._fallback.classify(query)
        if results:
            intent, confidence, _, _ = results[0]
            return intent, confidence, "keyword-based classification"
        return QueryIntent.GENERAL, 0.1, "no match found"

    def _llm_classify(self, query: str, provider: str, model_id: str) -> tuple:
        """Ask the LLM to classify the query. Returns (QueryIntent, confidence, reasoning)."""
        from finagent_core.registries import ModelsRegistry

        base_url = self.api_keys.get(f"{provider}_base_url") or self.api_keys.get(f"{provider.upper()}_BASE_URL")
        model = ModelsRegistry.create_model(
            provider=provider,
            model_id=model_id,
            api_keys=self.api_keys,
            temperature=0.0,  # Deterministic — routing must be consistent
            base_url=base_url or None,
        )

        from agno.agent import Agent
        router_agent = Agent(
            model=model,
            instructions=LLM_ROUTER_SYSTEM_PROMPT,
            markdown=False,
        )

        response = router_agent.run(f"Classify this query: {query}")

        # Extract text
        if hasattr(response, "content"):
            text = response.content
        else:
            text = str(response)

        # Strip any accidental markdown code fences
        text = text.strip()
        if text.startswith("```"):
            text = text.split("```")[1]
            if text.startswith("json"):
                text = text[4:]
        text = text.strip()

        parsed = json.loads(text)
        intent_str = parsed.get("intent", "general").lower()
        confidence = float(parsed.get("confidence", 0.8))
        reasoning = parsed.get("reasoning", "")

        # Validate intent value
        if intent_str not in self.INTENT_VALUES:
            intent_str = "general"

        intent = QueryIntent(intent_str)
        logger.info(f"LLM routed '{query[:60]}' → {intent_str} (conf={confidence:.2f}): {reasoning}")
        return intent, confidence, reasoning


class SuperAgent:
    """
    SuperAgent - Routes queries to appropriate sub-agents.

    Features:
    - LLM-driven intent classification (true agentic routing)
    - Keyword fallback when LLM unavailable
    - Multi-agent routing
    - Fallback handling
    - Response aggregation
    """

    def __init__(
        self,
        api_keys: Optional[Dict[str, str]] = None,
        routes: Optional[List[RouteConfig]] = None,
        model_config: Optional[Dict[str, Any]] = None,
    ):
        self.api_keys = api_keys or {}
        self.classifier = IntentClassifier()  # keyword fallback
        self.llm_router = LLMRouter(model_config=model_config, api_keys=self.api_keys)
        self.routes: Dict[QueryIntent, RouteConfig] = {}
        self.fallback_agent_id = "core_agent"

        # Initialize default routes
        self._setup_default_routes()

        # Apply custom routes
        if routes:
            for route in routes:
                self.add_route(route)

    def _setup_default_routes(self) -> None:
        """Setup default routing table"""
        default_routes = [
            RouteConfig(
                intent=QueryIntent.TRADING,
                agent_id="trading_agent",
                keywords=["buy", "sell", "trade"],
                priority=10,
                config_override={"tools": ["yfinance", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.PORTFOLIO,
                agent_id="portfolio_agent",
                keywords=["portfolio", "allocation"],
                priority=9,
                config_override={"tools": ["yfinance", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.ANALYSIS,
                agent_id="analysis_agent",
                keywords=["analyze", "valuation"],
                priority=8,
                config_override={"reasoning": True, "tools": ["yfinance", "duckduckgo"]}
            ),
            RouteConfig(
                intent=QueryIntent.RISK,
                agent_id="risk_agent",
                keywords=["risk", "volatility"],
                priority=8,
                config_override={"tools": ["yfinance", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.NEWS,
                agent_id="news_agent",
                keywords=["news", "headlines"],
                priority=5,
                config_override={"tools": ["duckduckgo"]}
            ),
            RouteConfig(
                intent=QueryIntent.GEOPOLITICS,
                agent_id="geopolitics_agent",
                keywords=["geopolitical", "conflict"],
                priority=5,
                config_override={"tools": ["duckduckgo"]}
            ),
            RouteConfig(
                intent=QueryIntent.ECONOMICS,
                agent_id="economics_agent",
                keywords=["gdp", "inflation"],
                priority=6,
                config_override={"tools": ["duckduckgo", "calculator"]}
            ),
            RouteConfig(
                intent=QueryIntent.RESEARCH,
                agent_id="research_agent",
                keywords=["research", "investigate"],
                priority=7,
                config_override={"reasoning": True, "tools": ["duckduckgo", "yfinance"]}
            ),
            RouteConfig(
                intent=QueryIntent.GENERAL,
                agent_id="core_agent",
                keywords=[],
                priority=0,
                config_override={}
            )
        ]

        for route in default_routes:
            self.routes[route.intent] = route

    def add_route(self, route: RouteConfig) -> None:
        """Add or update a route"""
        self.routes[route.intent] = route
        # Update classifier keywords
        if route.keywords:
            self.classifier.keywords.setdefault(route.intent, []).extend(route.keywords)

    def _resolve_model_config_from_keys(self, api_keys: Dict[str, Any]) -> Dict[str, Any]:
        """Derive provider + base_url from available api_keys instead of hardcoding openai."""
        keys = api_keys or {}
        preferred = ["fincept", "ollama", "anthropic", "google", "groq",
                     "deepseek", "openai", "openrouter"]
        for provider in preferred:
            if keys.get(provider) or keys.get(f"{provider.upper()}_API_KEY"):
                config = {"provider": provider}
                # Pick up custom base_url if shipped (e.g. MiniMax behind anthropic provider)
                base_url = keys.get(f"{provider}_base_url") or keys.get(f"{provider.upper()}_BASE_URL")
                if base_url:
                    config["base_url"] = base_url
                return config
        for k, v in keys.items():
            if k.endswith("_API_KEY") and v:
                provider = k[:-8].lower()
                config = {"provider": provider}
                base_url = keys.get(f"{provider}_base_url") or keys.get(f"{provider.upper()}_BASE_URL")
                if base_url:
                    config["base_url"] = base_url
                return config
        return {"provider": "openai"}

    def route(self, query: str) -> RoutingResult:
        """
        Route query to appropriate agent using LLM classification.
        Falls back to keyword matching if LLM is unavailable.

        Args:
            query: User query

        Returns:
            RoutingResult with agent info and confidence
        """
        intent, confidence, reasoning = self.llm_router.classify(query)
        route = self.routes.get(intent, self.routes.get(QueryIntent.GENERAL))

        return RoutingResult(
            intent=intent,
            agent_id=route.agent_id if route else self.fallback_agent_id,
            confidence=confidence,
            config=route.config_override if route else {},
            matched_keywords=[reasoning],  # repurpose field to carry LLM reasoning
            matched_patterns=[],
        )

    def route_multi(self, query: str, max_agents: int = 3) -> List[RoutingResult]:
        """
        Route to multiple agents for complex queries.

        Args:
            query: User query
            max_agents: Maximum number of agents to route to

        Returns:
            List of RoutingResults sorted by relevance
        """
        classifications = self.classifier.classify(query)
        results = []

        for intent, confidence, matched_kw, matched_pt in classifications[:max_agents]:
            if confidence < 0.2:  # Skip low confidence
                continue

            route = self.routes.get(intent)
            if route:
                results.append(RoutingResult(
                    intent=intent,
                    agent_id=route.agent_id,
                    confidence=confidence,
                    config=route.config_override,
                    matched_keywords=matched_kw,
                    matched_patterns=matched_pt
                ))

        return results

    def execute(
        self,
        query: str,
        session_id: Optional[str] = None,
        context: Optional[Dict[str, Any]] = None,
        user_config: Optional[Dict[str, Any]] = None,
        routing: Optional[RoutingResult] = None
    ) -> Dict[str, Any]:
        """
        Route and execute query.

        Args:
            query: User query
            session_id: Optional session ID
            context: Optional context
            user_config: Optional user-provided model config (from UI settings)
            routing: Optional pre-resolved routing result

        Returns:
            Response from routed agent
        """
        from finagent_core.core_agent import CoreAgent
        from finagent_core.agent_loader import get_loader

        # Pass user model config to LLM router so it can use the same provider
        if user_config and user_config.get("model"):
            self.llm_router.model_config = user_config["model"]

        # Route query (LLM-based) if not provided
        if not routing:
            routing = self.route(query)
        
        logger.info(f"Executing with agent {routing.agent_id} (intent: {routing.intent.value})")

        # Build config - use user's model config if provided, otherwise resolve from api_keys
        if user_config and user_config.get("model"):
            model_config = user_config["model"]
        else:
            model_config = self._resolve_model_config_from_keys(getattr(self, "api_keys", {}))

        config = {
            "model": model_config,
            "instructions": self._get_instructions_for_intent(routing.intent),
            **routing.config
        }

        # Merge user config overrides (tools, reasoning, etc.)
        if user_config:
            if user_config.get("tools"):
                config["tools"] = user_config["tools"]
            if user_config.get("reasoning") is not None:
                config["reasoning"] = user_config["reasoning"]
            if user_config.get("instructions"):
                config["instructions"] = user_config["instructions"]

        # Validate model config before execution
        model_provider = config.get("model", {}).get("provider", "")
        model_id = config.get("model", {}).get("model_id", "")
        if not model_provider or not model_id:
            return {
                "success": False,
                "error": "No LLM model configured. Please configure a model provider and model ID in Settings > LLM Configuration.",
                "routing": {
                    "intent": routing.intent.value,
                    "agent_id": routing.agent_id,
                    "confidence": routing.confidence,
                    "matched_keywords": routing.matched_keywords
                }
            }

        # Try to load specific agent
        loader = get_loader()
        try:
            agent = loader.create_agent(routing.agent_id, self.api_keys, config)
        except Exception:
            # Fallback to CoreAgent
            agent = CoreAgent(api_keys=self.api_keys)

        # Execute
        try:
            response = agent.run(query, config, session_id)
            content = agent.get_response_content(response) if hasattr(agent, 'get_response_content') else str(response)

            # Detect when Agno swallowed an API error and returned it as response text
            _ERROR_PREFIXES = (
                "Fincept API error",
                "Fincept API request timed out",
                "Fincept API failed",
                "Cannot connect to Fincept API",
            )
            if isinstance(content, str) and any(content.startswith(p) for p in _ERROR_PREFIXES):
                return {
                    "success": False,
                    "error": content,
                    "routing": {
                        "intent": routing.intent.value,
                        "agent_id": routing.agent_id,
                        "confidence": routing.confidence,
                        "matched_keywords": routing.matched_keywords
                    },
                    "model_attempted": f"{model_provider}/{model_id}"
                }

            return {
                "success": True,
                "response": content,
                "routing": {
                    "intent": routing.intent.value,
                    "agent_id": routing.agent_id,
                    "confidence": routing.confidence,
                    "matched_keywords": routing.matched_keywords
                },
                "model_used": f"{model_provider}/{model_id}"
            }
        except Exception as e:
            error_msg = str(e)
            # Provide helpful error for common issues
            if "api_key" in error_msg.lower() or "authentication" in error_msg.lower() or "unauthorized" in error_msg.lower():
                error_msg = f"API key error for provider '{model_provider}': {error_msg}. Check your API key in Settings > LLM Configuration."
            elif "connection" in error_msg.lower() or "connect" in error_msg.lower():
                error_msg = f"Connection error for '{model_provider}': {error_msg}. Check your base URL and network."

            return {
                "success": False,
                "error": error_msg,
                "routing": {
                    "intent": routing.intent.value,
                    "agent_id": routing.agent_id,
                    "confidence": routing.confidence
                },
                "model_attempted": f"{model_provider}/{model_id}"
            }

    def execute_multi(
        self,
        query: str,
        session_id: Optional[str] = None,
        aggregate: bool = True,
        user_config: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Execute query on multiple routed agents and aggregate results.

        Args:
            query: User query
            session_id: Optional session ID
            aggregate: Whether to aggregate responses
            user_config: Optional user-provided config

        Returns:
            Aggregated or individual responses
        """
        routings = self.route_multi(query)

        if not routings:
            return self.execute(query, session_id, user_config=user_config)

        responses = []
        for routing in routings:
            # Isolate sub-agent sessions to prevent turn violations in shared history (P4.23)
            sub_session_id = f"{session_id}_{routing.agent_id}" if session_id else None
            
            # Execute each routed agent specifically
            result = self.execute(query, sub_session_id, user_config=user_config, routing=routing)
            result["routing"]["priority"] = routing.confidence
            responses.append(result)

        if aggregate and len(responses) > 1:
            # Simple aggregation - combine successful responses
            combined = []
            for resp in responses:
                if resp.get("success"):
                    combined.append(f"[{resp['routing']['intent']}]\n{resp['response']}")

            return {
                "success": True,
                "response": "\n\n---\n\n".join(combined),
                "responses": responses,
                "aggregated": True
            }

        return {
            "success": True,
            "responses": responses,
            "aggregated": False
        }

    def _get_instructions_for_intent(self, intent: QueryIntent) -> str:
        """Get specialized instructions for intent"""
        instructions = {
            QueryIntent.TRADING: """你是一个交易助手。请在以下方面为用户提供帮助：
- 订单下达与执行
- 持仓管理
- 入场/出场策略
- 市场择时""",

            QueryIntent.PORTFOLIO: """你是一个投资组合经理。请在以下方面为用户提供帮助：
- 投资组合配置与再平衡
- 多元化策略
- 资产选择
- 头寸控制""",

            QueryIntent.ANALYSIS: """你是一个财务分析师。请在以下方面为用户提供帮助：
- 股票估值（DCF、可比公司法）
- 基本面分析
- 技术分析
- 市场研究""",

            QueryIntent.RISK: """你是一个风险分析师。请在以下方面为用户提供帮助：
- 风险指标（VaR、夏普比率等）
- 波动率分析
- 压力测试
- 对冲策略""",

            QueryIntent.NEWS: """你是一个财经新闻分析师。请在以下方面为用户提供帮助：
- 市场新闻与头条
- 盈利公告
- 公司更新
- 行业新闻""",

            QueryIntent.GEOPOLITICS: """你是一个地缘政治分析师。请在以下方面为用户提供帮助：
- 地缘政治风险评估
- 国际关系
- 政策分析
- 全球宏观趋势""",

            QueryIntent.ECONOMICS: """你是一个经济学家。请在以下方面为用户提供帮助：
- 经济指标
- 央行政策
- 宏观趋势
- 经济预测""",

            QueryIntent.RESEARCH: """你是一个研究员。请在以下方面为用户提供帮助：
- 深度研究
- 综合分析
- 行业研究
- 投资指南""",

            QueryIntent.GENERAL: """你是一个专业的金融 AI 助手。请始终使用中文回复用户。"""
        }

        return instructions.get(intent, instructions[QueryIntent.GENERAL])


# Entry point for C++ commands
def route_query(query: str, api_keys: Dict[str, str] = None, config: Dict[str, Any] = None) -> Dict[str, Any]:
    """Route a query and return routing info"""
    model_config = (config or {}).get("model")
    agent = SuperAgent(api_keys=api_keys, model_config=model_config)
    routing = agent.route(query)
    return {
        "success": True,
        "intent": routing.intent.value,
        "agent_id": routing.agent_id,
        "confidence": routing.confidence,
        "matched_keywords": routing.matched_keywords,
        "matched_patterns": routing.matched_patterns,
        "config": routing.config
    }


def execute_query(
    query: str,
    api_keys: Dict[str, str] = None,
    session_id: str = None,
    config: Dict[str, Any] = None
) -> Dict[str, Any]:
    """Route and execute a query"""
    model_config = config.get("model") if config else None
    agent = SuperAgent(api_keys=api_keys, model_config=model_config)
    return agent.execute(query, session_id, user_config=config)
