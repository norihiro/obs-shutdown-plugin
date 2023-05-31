'''
This example script will shutdown obs-studio.
'''

import obsws_python


def main():
    import sys
    cl = obsws_python.ReqClient(host='localhost', port=4455)

    cl.send('CallVendorRequest', {
        'vendorName': 'obs-shutdown-plugin',
        'requestType': 'shutdown',
        'requestData': {
            'reason': f'requested by {sys.argv[0]}',
			'support_url': 'https://github.com/norihiro/obs-shutdown-plugin/issues',
        },
    })


if __name__ == '__main__':
    main()
