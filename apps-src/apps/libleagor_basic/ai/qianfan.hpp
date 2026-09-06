#ifndef LIBROS_ROSE_QIANFAN_HPP_
#define LIBROS_ROSE_QIANFAN_HPP_


#include "rose_lua.hpp"
#include "base_slot.hpp"
#include "rose_net_api.hpp"
#include "rose_sdl_utils.hpp"
#include <chrono>

namespace aplt {

class DECLSPEC tnlp_model
{
public:
	tnlp_model(tai_slot& slot, const std::string& preferences_dir);
	virtual ~tnlp_model() {}

	void send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel);

	bool ds_retbool() const { return ds_retbool_; }
	const std::string& ds_answer() const { return ds_answer_; }
	int ds_input_tokens() const { return ds_input_tokens_; }
	int ds_output_tokens() const { return ds_output_tokens_; }

	void add_question_log(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf);

protected:
	virtual void app_send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel) = 0;


	virtual std::string handle_response(const Json::Value& json_object, bool& handled);
	void set_task_result(aplt::tb_api& ros, int src, bool new_conversation, bool retval, const std::string& answer, int input_tokens, int output_tokens, bool cancel);

	virtual void clear()
	{
		ds_retbool_ = false;
		ds_answer_.clear();
		ds_input_tokens_ = 0;
		ds_output_tokens_ = 0;
	}


protected:
	tai_slot& slot_;
	const std::string preferences_dir_;
	bool summary_into_aiagent_;

	bool ds_retbool_;
	std::string ds_answer_;
	int ds_input_tokens_;
	int ds_output_tokens_;
};

class tqianfan: public tnlp_model
{
public:
	tqianfan(tai_slot& slot, const std::string& preferences_dir);
	~tqianfan() {}

	void app_send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel) override;

private:	
	std::string xmit(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question);
	std::string xmit_with_img(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel);

	// void set_task_result(aplt::tb_api& ros, bool retval, const std::string& answer, bool cancel);

	bool did_pre_deepseek(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& appid, const std::string& model, const std::string& question);
	bool did_post_deepseek(net::thttp_api& net_api, int status, const std::string& data_received, std::string& answer);

	bool did_pre_new_conversation(net::thttp_api& net_api, std::string& body, const std::string& api_key, const std::string& app_id);
	bool did_post_new_conversation(net::thttp_api& net_api, int status, const std::string& data_received, std::string& conversation_id);

	bool did_pre_upload_file(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& app_id, const std::string& conversation_id, const surface& surf);
	bool did_post_upload_file(net::thttp_api& net_api, int status, const std::string& data_received, std::string& file_id);

	bool did_pre_conversation_run(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& app_id, const std::string& conversation_id, const std::string& file_id, const std::string& query);
	bool did_post_conversation_run(net::thttp_api& net_api, int status, const std::string& data_received, std::string& answer);

private:
	// const std::string preferences_dir_;

	// bool ds_retbool_;
	// std::string ds_answer_;
};

struct Message
{
    std::string role;
    std::string content;
    int tokens;
    std::chrono::system_clock::time_point timestamp;
    
    Message(const std::string& r, const std::string& c, int t = 0) 
        : role(r), content(c), tokens(t) {
        timestamp = std::chrono::system_clock::now();
    }
};

class tdeepseek;

class EfficientChatManager
{    
public:
    EfficientChatManager(tdeepseek& deepseek, int maxTokens = 1000)
        : deepseek_(deepseek)
		, maxHistoryTokens(maxTokens)
		, min_history_msgs_(1)
		, totalTokensUsed(0)
		, totalCost(0.0)
	{}

	std::string sendMessage(tb_api& ros, int src, bool new_conversation, const std::string& userMessage, bool& cancel);

	void printStatistics() const {
        SDL_Log("=== Token usage statistics ===");
        SDL_Log("Total Tokens: %i", totalTokensUsed);
        SDL_Log("Total cost(RBM): %.5f)", totalCost);
        SDL_Log("history messages: %i", (int)conversationHistory.size());
        SDL_Log("curr summary: %s", (conversationSummary.empty() ? "No" : conversationSummary.substr(0, 100) + "...").c_str());
    }
    
    void clearHistory()
	{
        conversationHistory.clear();
        conversationSummary.clear();
		totalTokensUsed = 0;
		totalTokensUsed = 0.0;
    }

private:
	std::vector<Message> prepareOptimizedMessages(const std::string& userMessage);
	void addSelectedHistory(std::vector<Message>& messages);
	int calculateMessagesTokens(const std::vector<Message>& messages);

	void updateCost(int inputTokens, int outputTokens) {
        double cost = (inputTokens * 0.001 + outputTokens * 0.002) / 1000.0;
        totalTokensUsed += inputTokens + outputTokens;
        totalCost += cost;
        
        SDL_Log("this tokens: %i tokens, input: %i, output: %i, cost(RBM): %.5f",
			inputTokens + outputTokens, inputTokens, outputTokens, cost);
    }

	void updateConversationHistory(const std::string& userMessage, 
                                 const std::string& assistantResponse,
                                 int tokensUsed);

	void cleanupOldMessages();

	bool needSummaryUpdate() const;

	void generateConversationSummary(tb_api& ros, bool& cancel);

private:
	tdeepseek& deepseek_;
    std::vector<Message> conversationHistory;
    std::string conversationSummary;
    int maxHistoryTokens;
	int min_history_msgs_;
    int totalTokensUsed;
    double totalCost;
};

class tdeepseek: public tnlp_model
{
public:
	tdeepseek(tai_slot& slot, const std::string& preferences_dir, threading::mutex& cpp_id_mutex);
	~tdeepseek() {}

	void app_send_question(aplt::tb_api& ros, int src, bool new_conversation, const std::string& question, const surface& surf, bool& cancel) override;

	void set_api_key(const std::string& api_key) { api_key_ = api_key; }
	std::string xmit2(aplt::tb_api& ros, int src, bool new_conversation, const std::vector<Message>& messages, bool& cancel);
	tai_slot& slot() { return slot_; }

private:
	bool did_pre_deepseek2(net::thttp_api& net_api, std::string& body, const std::string& api_key, 
		const std::string& model, const std::vector<Message>& messages);
	bool did_post_deepseek2(net::thttp_api& net_api, int status, const std::string& data_received, 
		std::string& answer, int& input_tokens, int& output_tokens);


private:
	threading::mutex& cpp_id_mutex_;
	std::string api_key_;
	EfficientChatManager EfficientChatManager_;
};

}

#endif // LIBROS_ROSE_DEEPSEEK_HPP_
