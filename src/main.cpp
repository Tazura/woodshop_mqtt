#include <mosquittopp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "config.h"

namespace bfs = boost::filesystem;
namespace bpo = boost::program_options;

typedef boost::lock_guard<boost::mutex> mutex_lock;

class App : private mosqpp::mosquittopp
{
public:
	enum
	{
		RULES_COUNT = 16
	};

	typedef struct SBroker
	{
		std::string host;
		int port;
		int keepalive;
		std::string username;
		std::string password;

	} SBroker;

	typedef struct SRule
	{
		int qos;
		std::string topicFrom;
		std::string topicTo;
		double threshold;
		std::string payload[2];
		int delay[2];
	} SRule;

	typedef struct SConfig
	{
		SBroker broker;
		std::map<std::string, SRule> rules;

	} SConfig;

private:
	typedef struct SState
	{
		enum EStatus
		{
			INIT,
			OFF,
			STARTING,
			ON,
			STOPPING
		} status = INIT;

		long long last_change = 0; // epoch in ms

	} SState;

public:
	App(const SConfig &a_config) : m_config(a_config)
	{
	}

	virtual ~App()
	{
	}

	int Connect()
	{
		if (!m_config.broker.username.empty())
		{
			if (username_pw_set(m_config.broker.username.c_str(), m_config.broker.password.c_str()))
			{
				fprintf(stderr, "Unable to set username/password\n");
				fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
				return 1;
			}
		}

		if (connect(m_config.broker.host.c_str(), m_config.broker.port, m_config.broker.keepalive))
		{
			fprintf(stderr, "Unable to connect to %s:%d\n", m_config.broker.host.c_str(), m_config.broker.port);
			fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
			return 1;
		}

		for (auto it = m_config.rules.begin(); it != m_config.rules.end(); it++)
		{
			const auto &rule = it->second;

			if (rule.topicFrom.empty() || rule.topicTo.empty())
				continue;

			if (subscribe(NULL, rule.topicFrom.c_str(), rule.qos))
			{
				fprintf(stderr, "Unable to subscribe to %s\n", rule.topicFrom.c_str());
				fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
				return 1;
			}
		}
		return 0;
	}

	int Run()
	{
		return loop_forever();
	}

protected:
	virtual void on_connect(int rc)
	{
	}
	virtual void on_disconnect(int rc)
	{
	}
	virtual void on_publish(int mid)
	{
	}
	virtual void on_message(const struct mosquitto_message *message)
	{
		if (message == NULL || message->topic == NULL || message->payload == NULL)
			return;

		//printf("MESSAGE: %s   -> %s\n", message->topic, message->payload ? message->payload : "{null}");

		auto itRule = m_config.rules.find(message->topic);
		if (itRule == m_config.rules.end())
		{
			fprintf(stderr, "No rule found for topic: %s\n", message->topic);
			fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
			return;
		}

		auto &rule = itRule->second;

		auto itState = m_states.find(rule.topicTo);
		if (itState == m_states.end())
		{
			itState = m_states.insert(std::make_pair(rule.topicTo, SState())).first;
		}

		auto &state = itState->second;

		auto value = fabs(atof((const char *)message->payload));

		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
		long long nowMS = ts.tv_sec;
		nowMS *= 1000;
		nowMS += ts.tv_nsec / 1000000;

		if (state.status == SState::INIT)
		{
			if (value >= rule.threshold)
			{
				state.status = SState::STARTING;
			}
			else
			{
				state.status = SState::STOPPING;
			}
			state.last_change = nowMS;
			fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
		}

		if (state.status == SState::OFF)
		{
			if (value >= rule.threshold)
			{
				state.status = SState::STARTING;
				state.last_change = nowMS;
				fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
			}
		}

		if (state.status == SState::ON)
		{
			if (value < rule.threshold)
			{
				state.status = SState::STOPPING;
				state.last_change = nowMS;
				fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
			}
		}

		if (state.status == SState::STARTING)
		{
			if (value >= rule.threshold)
			{
				if (nowMS - state.last_change >= rule.delay[0])
				{
					state.status = SState::ON;
					state.last_change = nowMS;

					auto &payload = rule.payload[0];
					publish(NULL, rule.topicTo.c_str(), payload.size() + 1, payload.c_str(), rule.qos);

					fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
				}
			}
			else
			{
				state.status = SState::OFF;
				fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
			}
		}

		if (state.status == SState::STOPPING)
		{
			if (value < rule.threshold)
			{
				if (nowMS - state.last_change >= rule.delay[1])
				{
					state.status = SState::OFF;
					state.last_change = nowMS;

					auto &payload = rule.payload[1];
					publish(NULL, rule.topicTo.c_str(), payload.size() + 1, payload.c_str(), rule.qos);

					fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
				}
			}
			else
			{
				state.status = SState::ON;

				fprintf(stdout, "%s:%d  %5.3f  %d\n", __FILE__, __LINE__, value, state.status);
			}
		}
	}
	virtual void on_subscribe(int mid, int qos_count, const int *granted_qos)
	{
	}
	virtual void on_unsubscribe(int mid)
	{
	}
	virtual void on_log(int level, const char *str)
	{
		switch (level)
		{
		//case MOSQ_LOG_DEBUG:
		//case MOSQ_LOG_INFO:
		//case MOSQ_LOG_NOTICE:
		case MOSQ_LOG_WARNING:
		case MOSQ_LOG_ERR:
			printf("LOG: %i:%s\n", level, str);
			break;

		default:
			break;
		}
	}
	virtual void on_error()
	{
		// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	}

private:
	boost::mutex m_mutext;
	const SConfig &m_config;
	std::map<std::string, SState> m_states;
};

int main(int argc, const char **argv)
{
	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	bfs::path configPath;

	bpo::options_description cmdLine("Command line options");
	cmdLine.add_options()														  /**/
		("config", bpo::value<bfs::path>(&configPath), "Configuration file path") /**/
		;
	bpo::variables_map vm;
	bpo::store(bpo::command_line_parser(argc, argv).options(cmdLine).allow_unregistered().run(), vm);
	bpo::notify(vm);

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	if (configPath.empty())
	{
		configPath = boost::dll::program_location().parent_path() / CONFIG_FILENAME;

		if (!bfs::exists(configPath))
			configPath = CONFIG_PATH;
		if (!bfs::exists(configPath))
			configPath = CONFIG_PATH_DEFAULT;
	}

	if (!bfs::exists(configPath))
	{
		fprintf(stderr, "Configuration not found: %s\n", configPath.c_str());
		fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
		return 1;
	}

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	std::ifstream ifs(configPath.native());
	if (!ifs)
	{
		fprintf(stderr, "Failed opening configuration: %s\n", configPath.c_str());
		fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
		return 1;
	}

	App::SConfig config;
	bpo::options_description config_options;
	config_options.add_options()																  /**/
		("Broker.Host", bpo::value<std::string>(&config.broker.host)->default_value("127.0.0.1")) /**/
		("Broker.Port", bpo::value<int>(&config.broker.port)->default_value(1883))				  /**/
		("Broker.KeepAlive", bpo::value<int>(&config.broker.keepalive)->default_value(60))		  /**/
		("Broker.Username", bpo::value<std::string>(&config.broker.username)->default_value(""))  /**/
		("Broker.Password", bpo::value<std::string>(&config.broker.password)->default_value(""))  /**/
		;

	std::vector<App::SRule> rules;
	rules.resize(App::RULES_COUNT);
	for (int i = 0; i < rules.size(); i++)
	{
		char name[256];

		sprintf(name, "Rule%d.QOS", i);
		config_options.add_options()(name, bpo::value<int>(&rules[i].qos)->default_value(0));

		sprintf(name, "Rule%d.TopicFrom", i);
		config_options.add_options()(name, bpo::value<std::string>(&rules[i].topicFrom));

		sprintf(name, "Rule%d.TopicTo", i);
		config_options.add_options()(name, bpo::value<std::string>(&rules[i].topicTo));

		sprintf(name, "Rule%d.Threshold", i);
		config_options.add_options()(name, bpo::value<double>(&rules[i].threshold));

		sprintf(name, "Rule%d.PayloadON", i);
		config_options.add_options()(name, bpo::value<std::string>(&rules[i].payload[0]));

		sprintf(name, "Rule%d.PayloadOFF", i);
		config_options.add_options()(name, bpo::value<std::string>(&rules[i].payload[1]));

		sprintf(name, "Rule%d.DelayON", i);
		config_options.add_options()(name, bpo::value<int>(&rules[i].delay[0]));

		sprintf(name, "Rule%d.DelayOFF", i);
		config_options.add_options()(name, bpo::value<int>(&rules[i].delay[1]));
	}

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	bpo::store(bpo::parse_config_file(ifs, config_options, true), vm);
	bpo::notify(vm);

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	for (auto it = rules.begin(); it != rules.end(); it++)
	{
		const auto &rule = *it;
		if (rule.topicFrom.empty() || rule.topicTo.empty())
			continue;

		config.rules[rule.topicFrom] = rule;
	}

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	mosqpp::lib_init();
	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	App app(config);
	if (app.Connect() != 0)
	{
		fprintf(stderr, "Failed to connect\n");
		fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	}
	else
	{
		fprintf(stderr, "Connected to MQTT broker\n");

		// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
		app.Run();
		// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	}

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	mosqpp::lib_cleanup();

	// fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	return 0;
}
