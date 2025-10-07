#include <iostream>
#include <csignal>
#include <memory>
#include "options.hpp"
#include "sms.hpp"
#include "error.hpp"


void sig_int_handler(int sig)
{
	std::cout << "Killed by ";
	if (sig == SIGINT) std::cout << " Ctrl-C ";
	if (sig == SIGTERM) std::cout << " KILL ";
	std::cout << std::endl;

	exit(0);
}

const std::string pdu1 = "07911356044902004412916801861326265746660008520113"
                         "1234718A8C050003D402013010660E65E565B9821F30110032"
                         "003000320035611F8C225E8651785F00542FFF01000A4EBA4EE"
                         "C65004E0A96EA5C71FF0C5411661F7A7A63A27D2230028C2262C"
                         "9683C8FCE676565B053D89769000A5728803662C951885FB77684"
                         "795D798F4E0BFF0C86548BDA65C54EBA518D6B2151FA53D1000A96"
                         "505B9A5E72545851DB5FA194F67070";

const std::string pdu2 = "0791135604490200441291680186132626574666000852011"
                         "31234718A8A050003D402023001003626055E725458572380"
                         "46521D96EA767B573A000A53C24E0E6D3B52A883B753D6003"
                         "626056D3B52A85E7254580026516C62DB5E72545830019650"
                         "5B9A53418FDE595652B1000A70B951FB00200068006700740"
                         "06F002E00630063002F0061002F0065004500590057002076"
                         "7B5F556E38620FFF0C62D265368BF756DE590D0052";

int main() 
{
    std::signal(SIGINT, sig_int_handler);
    std::signal(SIGTERM, sig_int_handler);
    std::signal(SIGABRT, Utils::Error::crash_printer);
    std::signal(SIGSEGV, Utils::Error::crash_printer);
    std::signal(SIGFPE, Utils::Error::crash_printer);
    std::signal(SIGILL, Utils::Error::crash_printer);
    std::signal(SIGBUS, Utils::Error::crash_printer);

	// std::cout << "===========\nReady for testing segmented SMS" << std::endl;
	// SMS message1(pdu2);
	// std::cout << "PDU2 -> Message 1 done" << std::endl;
	// std::cout << "===========\nFormulated dummy SMS (1) for test: " << message1 << std::endl;
	// message1.send_email(); // this should not send anything out, but print out log message
	// SMS message2(pdu1);
	// std::cout << "PDU2 -> Message 1 done" << std::endl;
	// std::cout << "===========\nFormulated dummy SMS (2) for test: " << message2 << std::endl;
	

	// std::cout << "===========\nNow sending email";

	// try
	// {
	// 	message1.send_email();
	// }
	// catch (const std::exception& e)
	// {
	// 	std::cerr << e.what() << std::endl;
	// }

	// std::cout << "===========\nNow sending your cutomized email";
	
	auto email_config = std::make_unique<Utils::Options::Email>();
	if (email_config == nullptr || !email_config->is_valid())
	{
		std::cerr << "Error! Email config has error";
		if (email_config) std::cerr << ": invalid email config";
		else std::cerr << ": nullptr";
		std::cerr << std::endl;
		return 1;
	}

	std::cout << "Test: email config is\nSender: " << email_config->get_sender() << " (\n\temail = "
		      << email_config->get_sender_email() << ",\n\temail(bracketed) = " << email_config->get_sender_bracket()
			  << ",\n\tserver = " << email_config->get_server() << ",\n\tpassword = " << email_config->get_password()
			  << "\n)\nReceiver: " << email_config->get_receiver() << "(\n\temail(bracketed) = "
			  << email_config->get_receiver_bracket() << "\n)" << std::endl;
	
	std::string sender_number;
	std::string content;
	std::cout << "Enter the phone number: ";
	std::getline(std::cin, sender_number);
	std::cout << "\nContent to " << sender_number << ": ";
	std::getline(std::cin, content);

	SMS message{sender_number, content};
	std::cout << "===========\nFormulated dummy SMS for test: " << message << std::endl;

	try
	{
		message.send_email();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

